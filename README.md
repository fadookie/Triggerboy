# Triggerboy
Triggerboy is an application for the [Arduino hardware platform](http://arduino.cc/) that is intended to interface with the [LittleSoundDJ](http://littlesounddj.com/) sequencer. It is intended as a tool to aid in the generation of live visuals for performances involving LSDJ. It analyzes various outputs from the Game Boy and triggers the digital out pins on the Arduino.

This is very much a work in progress!

## Triggers
Triggerboy is currently written around the concept of virtual triggers. Think of them as virtual switches or booleans that can be turned on or off depending on some condition.

Triggers can be mapped to physical digital output pins. This is done by editing the triggerMap array in  [Mode_Triggerboy.ino](Triggerboy/blob/master/Mode_Triggerboy.ino). Eventually I might add the capability to adjust mappings on the fly without re-flashing the software. 

### Current Triggers
The following triggers are currently implemented:

 * **Tick** - Pulses on every beat received from LSDJ. Defaults to a rate of 24 LSDJ clock pulses per quarter note.
 * **Tick Toggle** - Clone of tick trigger that toggles on the beat instead of pulsing.
 * **Amplitude** - Turns on for as long as the amplitude of the audio signal on Analog Pin 3 is above a certain threshold.
 * **Low-band Amplitude** - Turns on for as long as the amplitude of the audio signal is over a certain threshold in the "low band." The unit is currently running an 8-bit FFT, and I'm taking an average of a slice of it on the low end.
 * **High-band Amplitude** - Turns on for as long as the amplitude of the audio signal is over a certain threshold in the "high band."
 * **Test Clock** - Toggles once per second, this is for testing when no Game Boy is connected.
 * **Interrupt Test** - Fires whenever the interrupt handler on D2 is invoked.
 * **Null** - Triggers can be disabled with a simple code change by assigning them to the null trigger. If I implement on-the-fly trigger remapping, CPU cycles could be saved by assigning unused triggers to the null trigger at runtime.

### Planned Triggers
I'm currently planning or developing the following triggers:

 * **Mid-band Amplitude**
 *  **MIDI out triggers** - I may take advantage of the MIDI out functionality in Arduinoboy via a special build of LSDJ that sends MIDI notes from the Game Boy to the Arduino, as this would provide really tight sync with the music. Unfortunately, the current implementation requires a special command on each instrument in order to actually send the notes.

## Hardware
[Schematics may be found on the wiki](https://github.com/fadookie/Triggerboy/wiki/Hardware).

The circuit layout for Triggerboy is based on [the Arduinoboy schematic](http://trash80.com/arduinoboy/arduinoboy_schematic_1_1_0.png), with several changes:

 * The Game Boy clock line has been moved from A0 to D2. The rest of the Game Boy data lines are currently unused and do not need to be connected, but you do still need to ground the cable.
 * An amplified mono audio signal may be connected on A3 to drive audio-related triggers. More audio inputs are planned, but not yet implemented.
 * The status LED on D13 is now optional, as D13 is also assignable to triggers. All other LEDs have been removed to make room for more trigger outputs.
 * The MIDI In and MIDI Out components of the circuit have been removed, allowing serial communication with a computer via USB for debugging purposes. Therefore, the TX and RX pins are reserved.
 * The mode button has been disabled. D3 is currently reserved.
 * All other digital pins are available to be assigned to various triggers.
 
The layout is subject to change.

## Dependencies
Triggerboy requires the [8-bit Arduino FFT library](http://eliot.s3.amazonaws.com/media/visuals/triggerboy/ArduinoFFT.zip).

It is built and tested against the Arduino Software 1.0.1 and the Arduino UNO, though it may work on older boards.

## To-do
 * Implement mechanism for on-the-fly parameter adjustment to control triggers
 	* Possibly make parameters adjustable via MIDI or OpenSoundControl
 	* Or maybe just read some connected pots and switches

## Credits
Triggerboy is being developed by myself (Eliot Lash) and [Matt Payne](http://kineticturtle.blogspot.com/). Please direct questions to me.

Triggerboy is based heavily on [Arduinoboy v1.2.3](http://code.google.com/p/arduinoboy/) by Timothy Lamb. It uses the 8-bit version of  fix_fft.c by Tom Roberts et. al., taken from [the Arduino forum](http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1286718155).

## Special Thanks
 * Matt Payne for his continued encouragement, making awesome music and visuals, and inspiring me to dive into Arduino development.
 * [Bob Lash](http://www.bambi.net/bob.html), my dad, for mentoring me in electrical engineering and advising me on this project.
 * Timothy Lamb aka [Trash80](http://trash80.com/) for his support and assistance.
 * [Matt Rasmussen](https://twitter.com/mrasmus) for his assistance in debugging/optimizing LSDJ Master Sync to work simultaneously with analog sampling.

## License
Like Arduinoboy, Triggerboy is licensed under the GNU General Public License v2 or later. [See LICENSE file](Triggerboy/blob/master/LICENSE) for full agreement.
