
#include <LiquidCrystal_I2C.h>
#include <SparkFun_SDP3x_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_SDP3x
#include <Wire.h>
#include <Adafruit_ADS1015.h>//#include <Adafruit_ADS1X15.h> // Does not work on Windows :/ 
#include <Preferences.h>

/* HARDWARE */

// all connected to I2C: SDA: Pin 21, SCL: 22
SDP3X mySensor; //create an object of the SDP3X class
Adafruit_ADS1115 ads;  /* Use for the oxygensensor */
LiquidCrystal_I2C lcd(0x38, 16, 2); // set the LCD address to 0x3F for a 16 chars and 2 line display

/* CONFIGURABLE VALUES */

float oxy_m = 2.62; // needs to be calibrated
float oxy_b = 3;  // needs to be calibrated (corresponds to the voltage at Oxygen level == 0%)
static float airCalibrationValue = 20.9;
static float negativeFlowCalibrationValue = 100; // change this value to adjust the target oxygen concentration during negative flow calibration
float minAbsDiffPressure = 10; // needs to be calibrated (minimal absolute value of pressure to trigger negative flow calibration)

/* CONFIGURABLE BEHAVIOUR */

boolean shouldDoWarmupCalibration = true; //change this to false to skip warmup / calibration phase

/* CONFIGURABLE DELAYS */

unsigned static long warmupDelayInMs = 1000 * 10; // 10s warmup time before calibrating to 21% oxygen level
unsigned static long negativeFlowInitDelayInMs = 1000 * 2; // 2s before initiating negative flow calibration
unsigned static long calibrateNegativeFlowWarmupInMs = 1000 * 10; // 10s warmup time before negative flow calibration
unsigned static long negativeFlowPermanentSaveDelayInMs = 1000 * 10; // 10s delay before storing negative flow calibration value permanently

/* NON CONFIGURABLE */

int16_t adc0 = -1;
int16_t mv = 0;
float adcToMV =  0.1875;
unsigned long startTime;
unsigned long negativeFlowStartTime = 0;

struct CalibrationPoint {
  float oxygenLevel;
  float voltage; // in mV
};

CalibrationPoint calibrationA;
CalibrationPoint calibrationB;

unsigned static long infoMsgDelay = 1000 * 2; // 2s delay to display an info message

uint8_t countdown = 0;

float flowRate = 0;
float diffPressure = 0;
float oxygenLevel = 0;
float oxygenVoltageAtLaunch = 0;

enum DeviceState {
  LAUNCH,
  RESTORE_CALIBRATION,
  CALIBRATION_AIR_WARMUP,
  CALIBRATION_AIR,
  MEASURING,
  CALIBRATION_NEGFLOW_WARMUP,
  CALIBRATION_NEGFLOW_DONE,
  CALIBRATION_NEGFLOW_SAVE_DELAY,
  CALIBRATION_NEGFLOW_SAVED
};

enum DeviceState state = LAUNCH;

Preferences prefs;
const char* prefsDevice = "openOxy";
const char* prefsM = "m";
const char* prefsB = "b";


// TODO: Need to add a button to start the calibration
void setup() {

  Serial.begin(115200);

  Serial.println("Open Oxygen starting..");

  Wire.begin();

  // Initialize sensor
  mySensor.stopContinuousMeasurement();
  if (mySensor.begin() == false)
  {
    Serial.println(F("SDP3X not detected. Check connections. Freezing..."));
    while (1)
      ; // Do nothing more
  }
  mySensor.startContinuousMeasurement(true, true); // Request continuous measurements with mass flow temperature compensation and with averaging
  delay(1);

  // oxygen sensor related
  ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  ads.begin();
  // Setup 3V comparator on channel 0
  ads.startComparator_SingleEnded(0, 1000);


  // Init display
  lcd.init();
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(2, 1);
  lcd.print("     O2       ");
  lcd.setCursor(2, 0);
  lcd.print("Open Oxygen");

  // initial calibration points at zero and ambient
  calibrationA.oxygenLevel = airCalibrationValue;
  calibrationA.voltage = (airCalibrationValue - oxy_b) / oxy_m;
  calibrationB.oxygenLevel = 0;
  calibrationB.voltage = oxy_b;

  // set initial value for ambient voltage to something reasonable in case warmup is set to be skipped
  oxygenVoltageAtLaunch = calibrationA.voltage;

  if (!shouldDoWarmupCalibration) {
    state = MEASURING;
  }

  // read initial calibration values from flash
  if (openPreferences(true)) {
    float flashM = prefs.getFloat(prefsM, -1);
    float flashB = prefs.getFloat(prefsB, -1);
    Serial.print("Reading initial oxy_m value from flash: ");
    Serial.println(flashM);
    Serial.print("Reading initial oxy_b value from flash: ");
    Serial.println(flashB);
    if (flashM > -1 && flashB > -1) {
      oxy_m = flashM;
      oxy_b = flashB;
      calibrationA.voltage = (calibrationA.oxygenLevel - oxy_b) / oxy_m;
      calibrationB.voltage = (calibrationB.oxygenLevel - oxy_b) / oxy_m;
      state = RESTORE_CALIBRATION;
      shouldDoWarmupCalibration = false;
    }
    closePreferences();
  }

}

void loop() {

  long currentTime = millis();
  if (state == RESTORE_CALIBRATION) {
    if (currentTime - startTime > infoMsgDelay) {
      state = MEASURING;
    }
  } else if (shouldDoWarmupCalibration) {
    if (currentTime - startTime < warmupDelayInMs) {
      countdown = int((warmupDelayInMs - currentTime + startTime) / 1000) + 1;
      state = CALIBRATION_AIR_WARMUP;
    } else {
      // warmup is over, trigger calibration & skip warmup check in the future
      shouldDoWarmupCalibration = false;
      calibrationA.oxygenLevel = airCalibrationValue;
      updateCalibrationPointVoltage(calibrationA);
      oxygenVoltageAtLaunch = calibrationA.voltage;
      updateCalibration();
      state = MEASURING;
    }
  }

  // update sensor values
  updateFlowRate();
  updateOxygenLevel();


  // negative flow will trigger calibration of the non-ambient oxygen level
  if (state != CALIBRATION_AIR_WARMUP && state != RESTORE_CALIBRATION) {
    handleNegativeFlow();
  }

  displayState();

  delay(300);
}

void handleNegativeFlow() {
  if (diffPressure < 0 && abs(diffPressure) >= minAbsDiffPressure) {
    // negative flow rate detected
    long currentTime = millis();

    if (negativeFlowStartTime == 0) {
      // negative flow starting
      negativeFlowStartTime = currentTime;
    }
    int timeDif = currentTime - negativeFlowStartTime;

    // wait for init delay to trigger negative flow action
    unsigned long relevantDelay = negativeFlowInitDelayInMs;
    if (timeDif > relevantDelay) {

      // negative flow calibration

      // check if warmup is finished
      relevantDelay += calibrateNegativeFlowWarmupInMs;
      if(timeDif < relevantDelay) {
        state = CALIBRATION_NEGFLOW_WARMUP;
      } else {
        // check if calibration already happened, if not, calibrate and reset flash
        if(state == CALIBRATION_NEGFLOW_WARMUP) {
          calibrateToNegativeFlow();
          resetCalibrationInFlash();
          state = CALIBRATION_NEGFLOW_DONE;
        }

        // wait until "calibration done" message was displayed
        relevantDelay += infoMsgDelay;
        if (timeDif > relevantDelay) {

          // delay to confirm that the calibration should be permanently stored
          relevantDelay += negativeFlowPermanentSaveDelayInMs;
          if(timeDif < relevantDelay) {
            state = CALIBRATION_NEGFLOW_SAVE_DELAY;
          } else {
            // store calibration once
            if(state == CALIBRATION_NEGFLOW_SAVE_DELAY) {
              storeCalibrationToFlash();
              state = CALIBRATION_NEGFLOW_SAVED;
            }
          }
        }
      }
    }
    countdown = int((relevantDelay - timeDif) / 1000) + 1;
  } else {
    negativeFlowStartTime = 0;
  }
}

boolean openPreferences(boolean readOnly) {
  boolean success = prefs.begin(prefsDevice, readOnly);
  if (!success) Serial.println("failed to initialise NVS-Namespace");
  return success;
}

void closePreferences() {
  prefs.end();
}

void displayState() {
  String line1 = "  Open Oxygen   ";
  String line2 = "                ";

  if (state == LAUNCH) {
    line2 = "Launching..     ";
  }
  else if(state == CALIBRATION_AIR_WARMUP) {
    line1 = "Calibr. to ";
    line1 = line1 + String(airCalibrationValue, 1) + "%      ";
    line2 = "in ";
    line2 = line2 + countdown + "s, raw: " + adc0 * adcToMV + "        ";
  }
  else if (state == RESTORE_CALIBRATION) {
    line2 = "Restoring calibration..   ";
  }
  else if (state == MEASURING) {
    line1 = F("Flow: ");
    line1 += String(flowRate, 5) + "     ";
    line2 = F("Oxygen: ");
    line2 += String(oxygenLevel, 4) + "     ";
  }
  else if(state == CALIBRATION_NEGFLOW_WARMUP) {
    line1 = "Calibr. to ";
    line1 = line1 + String(negativeFlowCalibrationValue, 1) + "%     ";
    line2 = "in ";
    line2 = line2 + countdown + "s, raw: " + adc0 * adcToMV + "        ";
  }
  else if(state == CALIBRATION_NEGFLOW_DONE) {
    line1 = "Calibration to  ";
    line2 = "% O2 done.   ";
    line2 = String(negativeFlowCalibrationValue, 1) + line2;

  }
  else if(state == CALIBRATION_NEGFLOW_SAVE_DELAY) {
    line1 = "Saving calibr.  ";
    line2 = "perm. in ";
    line2 = line2 + countdown + "s..  ";
  }
  else if(state == CALIBRATION_NEGFLOW_SAVED) {
    line1 = "Calibr. to ";
    line1 = line1 + String(negativeFlowCalibrationValue, 1) + "%     ";
    line2 = "perm. saved.    ";
  }

  line1 = line1.substring(0, 16);
  line2 = line2.substring(0, 16);
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void updateFlowRate() {
  float temperature; // Storage for the temperature

  // Read the averaged differential pressure and temperature from the sensor
  mySensor.readMeasurement(&diffPressure, &temperature); // Read the measurement

  // convert the differential pressure into flow-rate
  flowRate = convert2slm(abs((float)diffPressure));

  Serial.print(F("Differential pressure is: "));
  Serial.print(diffPressure, 2);
  Serial.print(F(" (Pa);  Flow-rate: "));
  Serial.print(flowRate);
  Serial.println(F(" (slm)"));
}

void updateCalibrationPointVoltage(CalibrationPoint calibrationPoint) {
  float adcMean = 0; // we should do an average over many samples here
  for (int imeas = 0; imeas < 10; imeas++) {
    adc0 = ads.getLastConversionResults();
    adcMean += adc0;
  }
  adcMean = adcMean / 10;
  calibrationPoint.voltage = adcMean*adcToMV;
}

void updateCalibration() {
  oxy_m = (adcToMV * (calibrationA.voltage - calibrationB.voltage)) / (calibrationA.oxygenLevel - calibrationB.oxygenLevel);
  oxy_b = calibrationA.oxygenLevel - calibrationA.voltage*oxy_m;
  Serial.println("Update calibration using these points:");
  Serial.print("A: ");
  Serial.print(calibrationA.oxygenLevel);
  Serial.print("%, ");
  Serial.print(calibrationA.voltage);
  Serial.println("mV");
  Serial.print("B: ");
  Serial.print(calibrationB.oxygenLevel);
  Serial.print("%, ");
  Serial.print(calibrationB.voltage);
  Serial.println("mV");
  Serial.print("oxy_m: ");
  Serial.print(oxy_m);
  Serial.print(" oxy_b: ");
  Serial.println(oxy_b);
}


void updateOxygenLevel() {
  adc0 = ads.getLastConversionResults();
  //Serial.print("AIN0: "); Serial.println(adc0);
  // http://cool-web.de/esp8266-esp32/ads1115-16bit-adc-am-esp32-voltmeter.htm
  oxygenLevel = adc0 * adcToMV * oxy_m + oxy_b;
  Serial.print("Oxygenlevel: "); Serial.print(oxygenLevel); Serial.println(" %");
  Serial.print("ADC: "); 
  Serial.println(adc0);
}

void calibrateToNegativeFlow() {
    calibrationB.oxygenLevel = negativeFlowCalibrationValue;
    updateCalibrationPointVoltage(calibrationB);
    updateCalibration();
}

void storeCalibrationToFlash() {
  Serial.println("Writing calibration to flash:");
  if(openPreferences(false)) {
    prefs.putFloat(prefsM, oxy_m);
    prefs.putFloat(prefsB, oxy_b);
    Serial.print("Reading initial oxy_m value from flash: ");
    Serial.println(prefs.getFloat(prefsM, 0));
    Serial.print("Reading initial oxy_b value from flash: ");
    Serial.println(prefs.getFloat(prefsB, 0));
    closePreferences(); 
  }
}

void resetCalibrationInFlash() {
  Serial.println("Resetting oxy_m in flash..");
  if(openPreferences(false)) {
    prefs.remove(prefsM);
    prefs.remove(prefsB);
    Serial.print("Reading initial oxy_m value from flash: ");
    Serial.println(prefs.getFloat(prefsM, 0));
    Serial.print("Reading initial oxy_b value from flash: ");
    Serial.println(prefs.getFloat(prefsB, 0));
    closePreferences(); 
  }
}

float convert2slm(float dp) {
  // convert the differential presure dp into the standard liter per minute
  float a = -20.04843438;
  float b = 59.52168936;
  float c = 3.11050553;
  float d = 10.35186327;
  return a + sqrt(b + dp * d) * c;
}
