/*
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

 */
#include <SPI.h>
#include <MySensor.h>

//*No longer need the EEPROM.h?
//#include <EEPROM.h>

#define NODE_ID 24
#define SN "Rain Gauge"
#define SV "1.01b"

#define CHILD_ID_RAIN 3 
#define CHILD_ID_TRIPPED_RAIN 4

#define STATE_LOCATION 0 // location to save state to EEPROM
#define EEPROM_BUFFER 1  // location of the EEPROM circular buffer
#define BUFFER_LENGTH 121  // buffer plus the current hour
//
MySensor gw;
//
MyMessage msgRainRate(CHILD_ID_RAIN, V_RAINRATE);
MyMessage msgRain(CHILD_ID_RAIN, V_RAIN);
MyMessage msgRainVAR1(CHILD_ID_RAIN, V_VAR1);
MyMessage msgRainVAR2(CHILD_ID_RAIN, V_VAR2);
MyMessage msgRainVAR3(CHILD_ID_RAIN, V_VAR3);
MyMessage msgRainVAR4(CHILD_ID_RAIN, V_VAR4);
MyMessage msgRainVAR5(CHILD_ID_RAIN, V_VAR5);

MyMessage msgTripped(CHILD_ID_TRIPPED_RAIN, V_TRIPPED);
MyMessage msgTrippedVar1(CHILD_ID_TRIPPED_RAIN, V_VAR1);
MyMessage msgTrippedVar2(CHILD_ID_TRIPPED_RAIN, V_VAR2);
//
boolean metric = false;
//
int eepromIndex;
int tipSensorPin = 3; //Do not change (needed for interrupt)
int ledPin = 5; //PWM capable required
unsigned long dataMillis;
unsigned long serialInterval = 10000UL;
const unsigned long oneHour = 3600000UL;
unsigned long lastTipTime;
unsigned long lastBucketInterval;
unsigned long startMillis;

float rainBucket [120] ; // 5 days of data  //*120 hours = 5 days

float calibrateFactor = .7; //Calibration in milimeters of rain per single tip.  Note: Limit to one decimal place or data may be truncated when saving to eeprom. 

float rainRate = 0;
volatile int tipBuffer = 0;
byte rainWindow = 72;         //default rain window in hours
int rainSensorThreshold = 10; //default rain sensor sensitivity in mm.  Will be overwritten with msgTrippedVar2. 
byte state = 0;
byte oldState = -1;
//
void setup()
{
  gw.begin(getVariables, NODE_ID);

  pinMode(tipSensorPin, INPUT);

//  attachInterrupt (1, sensorTipped, CHANGE); //* should this be FALLING instead??
  attachInterrupt (1, sensorTipped, FALLING);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
  digitalWrite(tipSensorPin, HIGH); //ADDED. Activate internal pull-up 
  gw.sendSketchInfo(SN, SV);
  gw.present(CHILD_ID_RAIN, S_RAIN);
  gw.present(CHILD_ID_TRIPPED_RAIN, S_MOTION);
//  Serial.println(F("Sensor Presentation Complete"));
  

  state = gw.loadState(STATE_LOCATION); //retreive prior state from EEPROM
//  Serial.print("Tripped State (from EEPROM): ");
//  Serial.println(state);
  
  gw.send(msgTripped.set(state==1?"1":"0"));
  delay(250);                             // recharge the capacitor
  //
  boolean isDataOnEeprom = false;
  for (int i = 0; i < BUFFER_LENGTH; i++)
  {

    byte locator = gw.loadState(EEPROM_BUFFER + i); //New code

    if (locator == 0xFF)  // found the EEPROM circular buffer index
    {
      eepromIndex = EEPROM_BUFFER + i;
      //Now that we have the buffer index let's populate the rainBucket with data from eeprom
      loadRainArray(eepromIndex); 
      isDataOnEeprom = true;
//      Serial.print("EEPROM Index ");
//      Serial.println(eepromIndex);
      break;
    }
  }

  // reset the timers
  dataMillis = millis();
  startMillis = millis();
  lastTipTime = millis() - oneHour;  //will this work if millis() starts a 0??
  gw.request(CHILD_ID_TRIPPED_RAIN, V_VAR1); //Get rainWindow from controller (Vera)
  delay(250);
  gw.request(CHILD_ID_TRIPPED_RAIN, V_VAR2); //Get rainSensorThreshold from controller (Vera)
  delay(250);
//  Serial.println("Radio Done");
//  analogWrite(ledPin, 20);

}
//
void loop()
{
  gw.process();
  pulseLED();

  //
  // let's constantly check to see if the rain in the past rainWindow hours is greater than rainSensorThreshold
  //

  float measure = 0; // Check to see if we need to show sensor tripped in this block

  for (int i = 0; i < rainWindow; i++)
  {
    measure += rainBucket [i];
//    Serial.print("measure value (total rainBucket within rainWindow): ");
//    Serial.println(measure);
  }
  state = (measure >= rainSensorThreshold);

  if (state != oldState)
  {
    gw.send(msgTripped.set(state==1?"1":"0"));
    delay(250);
    gw.saveState(STATE_LOCATION, state); //New Code
//    Serial.print("State Changed. Tripped State: ");
//    Serial.println(state);
    
    oldState = state;
  }
  //
  // Now lets reset the rainRate to zero if no tips in the last hour
  //
  if (millis() - lastTipTime >= oneHour)// timeout for rain rate
  {
    if (rainRate != 0)
    {
      rainRate = 0;
      gw.send(msgRainRate.set(0));
      delay(250);
    }
  }

////////////////////////////
////Comment back in this block to enable Serial prints
//  
//  if ( (millis() - dataMillis) >= serialInterval)
//  {
//    for (int i = 24; i <= 120; i = i + 24)
//    {
//      updateSerialData(i);
//    }
//    dataMillis = millis();
//  }
//  
////////////////////////////
  
  
  if (tipBuffer > 0)
  {
//     Serial.println("Sensor Tipped");
    

    //******Added calibrateFactor calculations here to account for calibrateFactor being different than 1
    rainBucket [0] += calibrateFactor;
//    Serial.print("rainBucket [0] value: ");
//    Serial.println(rainBucket [0]);
    
    if (rainBucket [0] * calibrateFactor > 253) rainBucket[0] = 253; // odd occurance but prevent overflow}}

    float dayTotal = 0;
    for (int i = 0; i < 24; i++)
    {
      dayTotal = dayTotal + rainBucket [i];
    }
    
//    Serial.print("dayTotal value: ");
//    Serial.println(dayTotal);
    
    gw.send(msgRain.set(dayTotal,1));
    delay(250);
    unsigned long tipDelay = millis() - lastTipTime;
    if (tipDelay <= oneHour)
    {

      rainRate = ((oneHour) / tipDelay) * calibrateFactor; //Is my math/logic correct here??
  
      gw.send(msgRainRate.set(rainRate, 1));

//      Serial.print("RainRate= ");
//      Serial.println(rainRate);
    }

    //If this is the first trip in an hour send .1
    else
    {
      gw.send(msgRainRate.set(0.1, 1));
    }
    lastTipTime = millis();
    tipBuffer--;
  }
  if (millis() - startMillis >= oneHour)
  {
//    Serial.println("One hour elapsed.");
    //EEPROM write last value
    //Converting rainBucket to byte.  Note: limited to one decimal place.
    //Can this math be simplified?? 
    float convertRainBucket = rainBucket[0] * 10;
    if (convertRainBucket  > 253) convertRainBucket = 253; // odd occurance but prevent overflow
    byte eepromRainBucket = (byte)convertRainBucket;
    gw.saveState(eepromIndex, eepromRainBucket);

    eepromIndex++;
    if (eepromIndex > EEPROM_BUFFER + BUFFER_LENGTH) eepromIndex = EEPROM_BUFFER;
//    Serial.print("Writing to EEPROM.  Index: ");
//    Serial.println(eepromIndex);
    gw.saveState(eepromIndex, 0xFF);
    
    for (int i = BUFFER_LENGTH - 1; i >= 0; i--)//cascade an hour of values
    {
      rainBucket [i + 1] = rainBucket [i];
    }
    rainBucket[0] = 0;
    gw.send(msgRain.set(tipCounter(24),1));// send 24hr tips
    delay(250);
    transmitRainData(); // send all of the 5 buckets of data to controller
    startMillis = millis();
  }
}
//
void sensorTipped()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200)
  {
    tipBuffer++;
  }
  last_interrupt_time = interrupt_time;
}
//
float tipCounter(int hours)
{
  float tipCount = 0;
  for ( int i = 0; i < hours; i++)
  {
    tipCount = tipCount + rainBucket [i];
  }
  return tipCount;
}
//
void updateSerialData(int x)
{
  Serial.print(F("Tips last "));
  Serial.print(x);
  Serial.print(F(" hours: "));
  float tipCount = 0;
  for (int i = 0; i < x; i++)
  {
    tipCount = tipCount + rainBucket [i];
  }
  Serial.println(tipCount);
}
void loadRainArray(int value)
{
  for (int i = 0; i < BUFFER_LENGTH - 1; i++)
  {
    value--;
    Serial.print("EEPROM location: ");
    Serial.println(value);
    if (value < EEPROM_BUFFER)
    {
      value = EEPROM_BUFFER + BUFFER_LENGTH;
    }
    float rainValue = gw.loadState(value);

    if (rainValue < 255)
    {
      //Convert back to decimal value
      float decimalRainValue = rainValue/10;
      rainBucket[i] = decimalRainValue;
    }
    else
    {
      rainBucket [i] = 0;
    }
//    Serial.print("rainBucket[ value: ");
//    Serial.print(i);
//    Serial.print("] value: ");
//    Serial.println(rainBucket[i]);
  }
}

void transmitRainData(void)
{
  float rainUpdateTotal = 0;
  for (int i = 0; i < 24; i++)
  {
    rainUpdateTotal += rainBucket[i];
  }
  gw.send(msgRainVAR1.set(rainUpdateTotal,1));
  delay(250);
  for (int i = 24; i < 48; i++)
  {
    rainUpdateTotal += rainBucket[i];
  }
  gw.send(msgRainVAR2.set(rainUpdateTotal,1));
  delay(250);
  for (int i = 48; i < 72; i++)
  {
    rainUpdateTotal += rainBucket[i];
  }
  gw.send(msgRainVAR3.set(rainUpdateTotal,1));

  delay(250);
  for (int i = 72; i < 96; i++)
  {
    rainUpdateTotal += rainBucket[i];
  }
  gw.send(msgRainVAR4.set(rainUpdateTotal,1));

  delay(250);
  for (int i = 96; i < 120; i++)
  {
    rainUpdateTotal += rainBucket[i];
  }
  gw.send(msgRainVAR5.set(rainUpdateTotal,1));
  delay(250);
}

void getVariables(const MyMessage &message)
{
  if (message.sensor == CHILD_ID_RAIN)
  {
    // nothing to do here
  }
  else if (message.sensor == CHILD_ID_TRIPPED_RAIN)
  {
    if (message.type == V_VAR1)
    {
      rainWindow = atoi(message.data);
      if (rainWindow > 120)
      {
        rainWindow = 120;
      }
      else if (rainWindow < 6)
      {
        rainWindow = 6;
      }
      if (rainWindow != atoi(message.data))   // if I changed the value back inside the boundries, push that number back to Vera
      {
        gw.send(msgTrippedVar1.set(rainWindow));
      }
    }
    else if (message.type == V_VAR2)
    {
      rainSensorThreshold = atoi(message.data);
      if (rainSensorThreshold > 1000)
      {
        rainSensorThreshold = 1000;
      }
      else if (rainSensorThreshold < 1)
      {
        rainSensorThreshold = 1;
      }
      if (rainSensorThreshold != atoi(message.data))  // if I changed the value back inside the boundries, push that number back to Vera
      {
        gw.send(msgTrippedVar2.set(rainSensorThreshold));
      }
    }
  }
}
void pulseLED(void)
{
  static boolean ledState = true;
  static unsigned long pulseStart = millis();
  if (millis() - pulseStart < 500UL)
  {
    digitalWrite(ledPin, !ledState);
    pulseStart = millis();
  }
}
