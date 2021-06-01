# Open Oxygen (O2) Flow Sensor 

**WORK IN PROGRESS - more information will follow soon!**

This device is supposed to measure the oxygen flowrate of oxygen bottles.

<p align="left">
<a href="#logo" name="logo"><img src="./IMAGES/SETUP.jpeg" width="600"></a>
</p>

***Features:***

- Open-Source
- Low-Cost (~150€)
- Based on 3D parts + off-the-shelf components 
- Minimum of tools required 

# Electronics 

The flowsensor and the display are connected to the ESP32 via I2C (GPIO 21/22 for SDA/SCL), wheras the Oxygen Sensor is connected to the board via an ampliefied (i.e. OPAMP) digital-analog port. 

<p align="left">
<a href="#logo" name="Wiring"><img src="./IMAGES/WIRING.png" width="600"></a>
</p>

*WARNING:* The OPAMP has a high-pass filter created by a capacitor + resitor. We need to remove this to let the low-frequency signal from the Oxygen Sensor pass through. 

# Bill of materials 

- USB Cable 
- I2C LCD Display 
- LM386 Breakoutboard
- ESP32
- oxygen sensor oxiplus a 00a101
- [Sensirion Evaluationskit EK-P4, SDP3X](https://www.sensirion.com/de/durchflusssensoren/differenzdrucksensoren/differenzdrucksensor-sdp3x-mittels-evaluationskit-ek-p4-testen/)

# Contribute
If you have a question or found an error, please file an issue! We are happy to improve the device!

# License
Please have a look into the dedicated License file.

# Disclaimer
We do not give any guarantee for the proposed setup. Please use it at your own risk. Keep in mind that Laser source can be very harmful to your eye and your environemnt! It is not supposed to be used as a medical device! 
