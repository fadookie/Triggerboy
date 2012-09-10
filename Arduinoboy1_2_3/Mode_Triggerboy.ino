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

//FFT stuff
#define DATA_SIZE 128
char im[DATA_SIZE];
char data[DATA_SIZE];
#define AUDIO_IN_LEFT_PIN A3
//#define PRINT_FFT
//#define PRINT_TRIGGERS
//#define PRINT_AMPLITUDE_THRESH

#define NUM_TRIGGERS 2
#define TICK_TRIGGER 0
#define AMPLITUDE_TRIGGER 1
#define LOW_BAND_TRIGGER 2

#define AMPLITUDE_THRESH 700

byte triggerMap[NUM_TRIGGERS]; //Assignment of absolute triggers (index) to digital out port number (value)
boolean triggerStates[NUM_TRIGGERS]; //The current on/off state of each trigger
boolean pendingTriggerStates[NUM_TRIGGERS]; //Pending changes to on/off state of each trigger on the next update

void modeTriggerboySetup()
{
  logLine("Hello Triggerboy!");
  
  //Set up LSDJ Master Sync
  digitalWrite(pinStatusLed,LOW);
  DDRC  = B00000000; //Set analog in pins as inputs
  countSyncTime=0;
  blinkMaxCount=1000;
  
  //Set up trigger map.
  //triggerMap[0] = 13 means trigger 0 is assigned to pin 13, etc.
  triggerMap[TICK_TRIGGER]        = 4; //LSDJ Master Clock Ticks.
  triggerMap[AMPLITUDE_TRIGGER]    = 6; //Trigger when audio amplitude is over a certain threshhold
  
  //Configure all mapped outputs
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
	  for (i=0; i< 64;i++){
	     data[i] = sqrt(data[i] * data[i] + im[i] * im[i]);
	  }
	  
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
    switch(currentTrigger) {
      case TICK_TRIGGER:
        if (pendingTriggerStates[currentTrigger]) {
          pendingTriggerStates[currentTrigger] = false;
          if (didUpdate(currentTrigger, true)) stateChanged = true; //Update the current trigger state if needed
        } else {
          if (didUpdate(currentTrigger, false)) stateChanged = true;
        }
        break;
      case AMPLITUDE_TRIGGER:
      {
        int amp = analogRead(AUDIO_IN_LEFT_PIN);
        if (amp > AMPLITUDE_THRESH) {
#ifdef PRINT_AMPLITUDE_THRESH
          logTimestamp();
          Serial.print("AMPLITUDE = ");
          Serial.println(amp);
#endif
          if (didUpdate(currentTrigger, true)) stateChanged = true;
        } else {
          if (didUpdate(currentTrigger, false)) stateChanged = true;
        }
        break;
      }
      default:
        //Just use the pending value, don't modify it in case this is a continuous trigger
        if (didUpdate(currentTrigger, pendingTriggerStates[currentTrigger])) stateChanged = true;
        break;
    }
  }
  
#ifdef PRINT_TRIGGERS
  //Virtual triggering for debugging purposes:
  if (stateChanged) {
    logTimestamp();
    for (int currentTrigger = 0; currentTrigger < NUM_TRIGGERS; currentTrigger++) {
      byte currentPin = triggerMap[currentTrigger];
      boolean currentState = triggerStates[currentTrigger];
      Serial.print(currentPin);
      Serial.print(currentState ? "[*] " : "[ ] ");
    }
    Serial.println("");
  }
#endif

}

boolean didUpdate(byte trigger, boolean state) {
  //Does this trigger need an update? If so, update it and return true. Otherwise, return false.
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
