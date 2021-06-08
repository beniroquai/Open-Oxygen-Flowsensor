
#include <LiquidCrystal_I2C.h>
#include <SparkFun_SDP3x_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_SDP3x
#include <Wire.h>

SDP3X mySensor; //create an object of the SDP3X class

// read the voltage sensor
const int Analog_channel_pin= 15;
int ADC_VALUE = 0;
int voltage_value = 0; 

LiquidCrystal_I2C lcd(0x38,16,2);  // set the LCD address to 0x3F for a 16 chars and 2 line display


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

  // Initi display
  lcd.init();
  lcd.clear();         
  lcd.backlight();      // Make sure backlight is on
  
  // Print a message on both lines of the LCD.
  lcd.setCursor(2,0);   //Set cursor to character 2 on line 0
  lcd.print("     O2       ");
  
  lcd.setCursor(2,1);   //Move cursor to character 2 on line 1
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

  // readout oxygen level
  ADC_VALUE = analogRead(Analog_channel_pin);
  Serial.print("ADC VALUE = ");
  Serial.println(ADC_VALUE);
  delay(1000);
  voltage_value = (ADC_VALUE * 3.3 ) / (4095);
  Serial.print("Voltage = ");
  Serial.print(voltage_value);
  Serial.println("volts");
  delay(1000);


  // TODO: NEED TO combine the two strings properly!
  lcd.setCursor(0,0);   //Set cursor to character 2 on line 0
  lcd.print("Flow: ");
  lcd.setCursor(12,0);
  lcd.print(flowrate);

  lcd.setCursor(0,1);
  lcd.print("Oxygen (%): ");
  lcd.setCursor(12,1);   //Move cursor to character 2 on line 1
  lcd.print(voltage_value);

}


float convert2slm(float dp){
    // convert the differential presure dp into the standard liter per minute
    float a=-20.04843438;
    float b=59.52168936;
    float c=3.11050553;
    float d=10.35186327;
    return a+(b+dp*d)*c;
}
