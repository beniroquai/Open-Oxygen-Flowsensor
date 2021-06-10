
#include <LiquidCrystal_I2C.h>
#include <SparkFun_SDP3x_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_SDP3x
#include <Wire.h>
#include <Adafruit_ADS1015.h>


// all connected to I2C: SDA: Pin 21, SCL: 22
SDP3X mySensor; //create an object of the SDP3X class
Adafruit_ADS1115 ads;  /* Use for the oxygensensor */
LiquidCrystal_I2C lcd(0x38, 16, 2); // set the LCD address to 0x3F for a 16 chars and 2 line display


int16_t adc0;
int16_t mv = 0;
float oxy_m = 0.562; // needs to be calibrated
float oxy_b = 0;  // needs to be calibrated (corresponds to the voltage at Oxygen level == 0%)
int oxygen_level = 0;


// TODO: Need to add a button to start the calibration
void setup() {

  Serial.begin(115200);
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

  // oxygen sensor related
  ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)


  // Initi display
  lcd.init();
  lcd.clear();
  lcd.backlight();      // Make sure backlight is on

  // Print a message on both lines of the LCD.
  lcd.setCursor(2, 0);  //Set cursor to character 2 on line 0
  lcd.print("     O2       ");

  lcd.setCursor(2, 1);  //Move cursor to character 2 on line 1
  lcd.print("Open Oxygen");
}

void loop() {

  float diffPressure; // Storage for the differential pressure
  float temperature; // Storage for the temperature

  // Read the averaged differential pressure and temperature from the sensor
  mySensor.readMeasurement(&diffPressure, &temperature); // Read the measurement

  // convert the differential pressure into flow-rate
  float flowrate = convert2slm(diffPressure);

  Serial.print(F("Differential pressure is: "));
  Serial.print(diffPressure, 2);
  Serial.print(F(" (Pa);  Flow-rate: "));
  Serial.print(flowrate, 2);
  Serial.println(F(" (slm)"));



  // Read the ADC value from the bus:
  adc0 = ads.getLastConversionResults();
  //Serial.print("AIN0: "); Serial.println(adc0);
  oxygen_level = adc0 * 0.1875 * oxygen_level + oxy_b;

  Serial.print("Oxygenlevel: "); Serial.print(oxygen_level); Serial.println(" %");

  // TODO: NEED TO combine the two strings properly!
  lcd.setCursor(0, 0);  //Set cursor to character 2 on line 0
  lcd.print("Flow: ");
  lcd.setCursor(9, 0);
  lcd.print(flowrate);

  lcd.setCursor(0, 1);
  lcd.print("Oxygen: ");
  lcd.setCursor(9, 1);  //Move cursor to character 2 on line 1
  lcd.print(oxygen_level);

}


float convert2slm(float dp) {
  // convert the differential presure dp into the standard liter per minute
  float a = -20.04843438;
  float b = 59.52168936;
  float c = 3.11050553;
  float d = 10.35186327;
  return a + (b + dp * d) * c;
}
