/***************************************************************************
 * Name:    Eliot Lash                                                     *
 * Email:   eliot.lash@gmail.com                                           *
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <fix_fft.h>

// --------------------  Trigger config -------------------- //
const byte NUM_TRIGGERS = 1 + 1; //Set the number on the left to the # of triggers in use. The +1 is for the null trigger

const byte NULL_TRIGGER = 0; //DO NOT CHANGE!!! Triggers that are currently disabled can redirect their status changes to this trigger, so we don't have to disable the hooks for them throughout the code

//The in-use triggers should be continuous numbers from 1 through (NUM_TRIGGERS - 1.)
//Extras may be assigned to NULL_TRIGGER to disable them.
const byte TICK_TRIGGER = NULL_TRIGGER;
const byte AMPLITUDE_TRIGGER = NULL_TRIGGER;
const byte TEST_CLOCK_TRIGGER = NULL_TRIGGER;
const byte LOW_BAND_TRIGGER = 1;

//Config for each trigger:

//AMPLITUDE_TRIGGER
const int vAmplitudeThreshold = 700; //Voltage threshold for this trigger to turn on, this will be a value from analogRead() from 0 to 1023

//TEST_CLOCK_TRIGGER
const unsigned long msTestClockTickInterval = 1000; //How long to wait between test clock ticks (in milliseconds)

//LOW_BAND_TRIGGER
const char fftaThreshold = 5; //FFT amplitude threshold for this trigger to turn on

// --------------------  Pin config -------------------- //
const byte AUDIO_IN_LEFT_PIN = 3; //Read left channel audio from Analog In Pin 3.

// --------------------  Print debugging settings -------------------- //
//Uncomment a #define to enable printing to serial

//Print FFT every FFT frame:
//#define PRINT_FFT

//Print pinout state every time it changes:
//#define PRINT_TRIGGERS

//Print amplitude whenever the sample exceeds the threshold:
//#define PRINT_AMPLITUDE_THRESH

//Print low band FFT average:
//#define PRINT_LOW_BAND_AVG


// --------------------  Data Structures, etc. -------------------- //

//Trigger data structures
byte triggerMap[NUM_TRIGGERS]; //Assignment of absolute triggers (index) to digital out port number (value)
boolean triggerStates[NUM_TRIGGERS]; //The current on/off state of each trigger
boolean pendingTriggerStates[NUM_TRIGGERS]; //Pending changes to on/off state of each trigger on the next update

//FFT stuff
#define DATA_SIZE 128
char im[DATA_SIZE];
char data[DATA_SIZE];

void modeTriggerboySetup()
{
  logLine("Beginning setup of Triggerboy main mode.");
  
  //Set up LSDJ Master Sync
  digitalWrite(pinStatusLed,LOW);
  DDRC  = B00000000; //Set analog in pins as inputs
  countSyncTime=0;
  blinkMaxCount=1000;
  
  //Set up mapping between triggers and pinouts. It's okay for disabled triggers to be here since they will not fire.
  //triggerMap[0] = 13 means trigger 0 is assigned to pin 13, etc.
  triggerMap[TICK_TRIGGER]        = 4; //LSDJ Master Clock Ticks.
  triggerMap[AMPLITUDE_TRIGGER]   = 6; //Trigger when audio amplitude is over a certain threshhold
  triggerMap[TEST_CLOCK_TRIGGER]  = 13; //Trigger on an internal timer, for testing outputs independently of the connected inputs
  triggerMap[LOW_BAND_TRIGGER]    = 6; //Low-band FFT threshold trigger
  
  triggerMap[NULL_TRIGGER]        = 255; //A place for currently disabled triggers to dump data. This shouldn't be a real pin and should never be actually written to. Pinout defined after the other ones to overwrite them in case any of them are currently pointing to the null trigger.
  
  logTimestamp();
  Serial.print("There are currently ");
  Serial.print(NUM_TRIGGERS - 1);
  Serial.println(" active triggers:");
  
  //Configure all mapped pins as outputs
  for (int currentTrigger = 0; currentTrigger < NUM_TRIGGERS; currentTrigger++) {
    byte currentPin = triggerMap[currentTrigger];
    //Sanity check on reserved pins
    if (
     (usbMode && (0 == currentPin || 1 == currentPin))
     || pinButtonMode == currentPin
    ) {
      char errorMessage [150];
      sprintf(
        errorMessage,
        "FATAL ERROR: Trigger %i is assigned to pin %i, which is reserved.\nPlease fix your triggerMap config and re-flash the Arduino.",
        currentTrigger,
        currentPin
      );
      fatalError(errorMessage);
    }
    pinMode(currentPin, OUTPUT);
  }
  
  printTriggers();
  
  modeTriggerboy();
}

void modeTriggerboy()
{
  while(1){
    //Process LSDJ Master Sync
    if (Serial.available()) {                  //If serial data was send to midi input
      incomingMidiByte = Serial.read();            //Read it
      if(!checkForProgrammerSysex(incomingMidiByte) && !usbMode) Serial.write(incomingMidiByte);        //Send it to the midi output
    }
    readgbClockLine = PINC & 0x01; //Read gameboy's clock line
    if(readgbClockLine) {                          //If Gb's Clock is On
      while(readgbClockLine) {                     //Loop untill its off
        readgbClockLine = PINC & 0x01;            //Read the clock again
        bit = (PINC & 0x04)>>2;                   //Read the serial input for song position
        tb_checkActions();
        alwaysRunActions(); //Do stuff that should happen on every loop
      }
      
      countClockPause= 0;                          //Reset our wait timer for detecting a sequencer stop
      
      readGbSerialIn = readGbSerialIn << 1;        //left shift the serial byte by one to append new bit from last loop
      readGbSerialIn = readGbSerialIn + bit;       //and then add the bit that was read

      tb_sendMidiClockSlaveFromLSDJ();                //send the clock & start offset data to midi
      
    } else {
      //Still do stuff if we are waiting for the GB clock, i.e. if it's disconnected
      alwaysRunActions();
    }
    
    setMode();
  }
}

void fft_forward() {
  //Process the FFT for this loop
  int static i = 0;
  static long tt;
  int val;
  
   if (millis() > tt){
	if (i < DATA_SIZE){
	  val = analogRead(AUDIO_IN_LEFT_PIN);
	  data[i] = val / 4 - DATA_SIZE;
	  im[i] = 0;
	  i++;  
	  
	}
	else{
	  //this could be done with the fix_fftr function without the im array.
	  fix_fft(data,im,7,0);
	  // I am only interessted in the absolute value of the transformation
          int lowBandSum = 0;
          int lowBandAvgDenominator = 0;
	  for (i=0; i< 64;i++){
	     data[i] = sqrt(data[i] * data[i] + im[i] * im[i]);
             if (i >= 2 && i <= 10) {
               //Considering this the "low band" for now.
               lowBandSum += data[i];
               lowBandAvgDenominator++;
             }
	  }
          //Compute mean average amplitude of the low band
          int lowBandAvg = lowBandSum / lowBandAvgDenominator;
          //If this band is above our threshold for the LOW_BAND_TRIGGER, trigger on, otherwise trigger off
          pendingTriggerStates[LOW_BAND_TRIGGER] = (lowBandAvg > fftaThreshold);
          
#ifdef PRINT_LOW_BAND_AVG
          logTimestamp();
          Serial.print("low band mean average = ");
          Serial.println(lowBandAvg);
#endif
	  
	  //do something with the data values 1..64 and ignore im
#ifdef PRINT_FFT
	  print_fft(data);
#endif
	}
    
    tt = millis();
   }
}

#ifdef PRINT_FFT
void print_fft(char * data) {
  //Print FFT to serial for debugging
  //for (int i = 0; i < sizeof(data) / sizeof(char); i++) {
  for (int i = 0; i < 64; i++) {
    Serial.print(data[i], DEC);
    Serial.print(" ");
    //Serial.println(i);
  }
  Serial.println("");
}
#endif

void alwaysRunActions() {
  //Run these every single loop
  fft_forward();         //run the FFT
  triggerShit();
}

void tb_checkActions()
{
  tb_checkLSDJStopped();                        //Check if LSDJ hit Stop
  setMode();
  updateStatusLight();
}

void triggerShit() {
  //Trigger some outputs!
  
  //Set trigger states
  boolean stateChanged = false;
  for (int currentTrigger = 0; currentTrigger < NUM_TRIGGERS; currentTrigger++) {
    byte currentPin = triggerMap[currentTrigger];

    if (NULL_TRIGGER == currentTrigger) {
      //No-op
      
    } else if (TEST_CLOCK_TRIGGER == currentTrigger) {
      static unsigned long msLastTestClockTick;
      unsigned long msCurrent = millis();
      if ((msCurrent - msLastTestClockTick) >= msTestClockTickInterval) {
        //Toggle pin every clock tick
        if (didUpdate(currentTrigger, !triggerStates[currentTrigger])) stateChanged = true;
        msLastTestClockTick = msCurrent;
      }
      
    } else if (TICK_TRIGGER == currentTrigger) {
      if (pendingTriggerStates[currentTrigger]) {
        pendingTriggerStates[currentTrigger] = false;
        if (didUpdate(currentTrigger, true)) stateChanged = true; //Update the current trigger state if needed
      } else {
        if (didUpdate(currentTrigger, false)) stateChanged = true;
      }

    } else if (AMPLITUDE_TRIGGER == currentTrigger) {
      int amp = analogRead(AUDIO_IN_LEFT_PIN);
      if (amp > vAmplitudeThreshold) {
#ifdef PRINT_AMPLITUDE_THRESH
        logTimestamp();
        Serial.print("AMPLITUDE = ");
        Serial.println(amp);
#endif
        if (didUpdate(currentTrigger, true)) stateChanged = true;
      } else {
        if (didUpdate(currentTrigger, false)) stateChanged = true;
      }
    
     //DEFAULT TRIGGER BEHAVIOR:
     } else {
      //Just use the pending value without modifying it (in case it's continuous)
      if (didUpdate(currentTrigger, pendingTriggerStates[currentTrigger])) stateChanged = true;
    }
  }
  
#ifdef PRINT_TRIGGERS
  if (stateChanged) {
    printTriggers();
  }
#endif

}

/**
 * Print information on trigger->pinout assignments and pin HIGH/LOW state for debugging purposes
 */
void printTriggers() {
  logTimestamp();
  for (int currentTrigger = 0; currentTrigger < NUM_TRIGGERS; currentTrigger++) {
    byte currentPin = triggerMap[currentTrigger];
    boolean currentState = triggerStates[currentTrigger];
    if (currentTrigger != NULL_TRIGGER) {
      Serial.print("T");
      Serial.print(currentTrigger);
    } else {
      //Just to clarify that this isn't a normal trigger, we'll give it a special name
      //Serial.print("TNULL");
      //Actually, let's just not show the null trigger at all, since it's now guaranteed to never write to an actual pinout
      continue;
    }
    Serial.print("->D");
    Serial.print(currentPin);
    Serial.print(currentState ? "[*] " : "[ ] ");
  }
  Serial.println(" END"); //I was seeing a bug with just using an empty string here where the serial monitor was appearing to get stuck in a loop, adding END seems to fix it for some reason.
}

boolean didUpdate(byte trigger, boolean state) {
  //Does this trigger need an update? If so, update it and return true. Otherwise, return false.
  if (trigger == NULL_TRIGGER) return false;  //Skip the null trigger
  
  if (state != triggerStates[trigger]) {
    triggerStates[trigger] = state;
    digitalWrite(triggerMap[trigger], state ? HIGH : LOW);
    return true;
  } else {
    return false;
  }
}

 /*
  tb_checkLSDJStopped counts how long the clock was on, if its been on too long we assume 
  LSDJ has stopped- Send a MIDI transport stop message and return true.
 */
boolean tb_checkLSDJStopped()
{
  countClockPause++;                                 //Increment the counter
  if(countClockPause > 16000) {                      //if we've reached our waiting period
    if(sequencerStarted) {
      readgbClockLine=false;
      countClockPause = 0;                           //reset our clock
      if (!usbMode) Serial.write(0xFC);                      //send the transport stop message
      sequencerStop();                               //call the global sequencer stop function
    }
    return true;
  }
  return false;
}

 /*
  tb_sendMidiClockSlaveFromLSDJ waits for 8 clock bits from LSDJ,
  sends the transport start command if sequencer hasnt started yet,
  sends the midi clock tick, and sends a note value that corrisponds to
  LSDJ's row number on start (LSDJ only sends this once when it starts)
 */
void tb_sendMidiClockSlaveFromLSDJ()
{
  if(!countGbClockTicks) {      //If we hit 8 bits
    if(!sequencerStarted) {         //If the sequencer hasnt started
      if (!usbMode) {
        Serial.write((0x90+memory[MEM_LSDJMASTER_MIDI_CH])); //Send the midi channel byte
        Serial.write(readGbSerialIn);                //Send the row value as a note
        Serial.write(0x7F);                          //Send a velocity 127
        
        Serial.write(0xFA);     //send MIDI transport start message 
      }
      sequencerStart();             //call the global sequencer start function
    }
    if (usbMode) {
      //logLine("Triggerboy: Tick");
      pendingTriggerStates[TICK_TRIGGER] = true;
    } else {
      Serial.write(0xF8);       //Send the MIDI Clock Tick
    }
    
    countGbClockTicks=0;            //Reset the bit counter
    readGbSerialIn = 0x00;                //Reset our serial read value
    
    updateVisualSync();
  }
  countGbClockTicks++;              //Increment the bit counter
 if(countGbClockTicks==8) countGbClockTicks=0; 
}

void fatalError(const char * error) {
  if (usbMode) {
    Serial.println(error);
  }
  while(1) { //endless loop to stop main program from looping again
    //Flash an SOS
    morseBlink("sos ");
    delay(3000);
  }
}
