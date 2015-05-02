# MySensors_RainGage
Tipping Bucket Rain Gage for MySensors

Arduino Tipping Bucket Rain Gauge

 April 26, 2015

 Version 1.01b for MySensors version 1.4.1

 Arduino Tipping Bucket Rain Gauge

 Utilizing a tipping bucket sensor, your Vera home automation controller and the MySensors.org
 gateway you can measure and sense local rain.  This sketch will create two devices on your
 Vera controller.  One will display your total precipitation for the last 24, 48, 72, 96 and 120
 hours.  The other, a sensor that changes state if there is recent rain (up to last 120 hours)
 above a threshold.  Both these settings are user definable.

 This sketch features the following:

 * Allows you to set the rain threshold in mm
 * Allows you to determine the interval window up to 120 hours.
 * Displays the last 24, 48, 72, 96 and 120 hours total rain in Variable1 through Variable5
   of the Rain Sensor device
 * Configuration changes to Sensor device updated every 3 hours.
 * SHould run on any Arduino
 * Will retain Tripped/Not Tripped status and data in a power outage, saving small ammount
   of data to EEPROM (Circular Buffer to maximize life of EEPROM)
 * There is a unique setup requirement necessary in order to properly present the Vera device
   variables.  The details are outlined in the sketch below.
 * LED status indicator

 by BulldogLowell@gmail.com for free public use
