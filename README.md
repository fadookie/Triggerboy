# Triggerboy
Triggerboy is an application for the [Arduino hardware platform](http://arduino.cc/) that is intended to interface with [LittleSoundDJ](http://littlesounddj.com/) sequencer. It is intended as a tool to aid in the generation of live visuals for performances involving LSDJ. It analyzes various outputs from the Game Boy and triggers the digital out pins on the Arduino.

This is very much a work in progress!

## Triggers
Triggerboy is currently written around the concept of virtual triggers. Think of them as virtual switches or booleans that can be turned on or off depending on some condition.

Triggers can be mapped to physical digital output pins. This is done by editing the triggerMap array in  [Mode_Triggerboy.ino](Triggerboy/blob/master/Mode_Triggerboy.ino). Eventually I might add the capability to adjust mappings on the fly without re-flashing the software. 

### Current Triggers
The following triggers are currently implemented:

 * **Tick** - Pulses on for every clock tick received from LSDJ.
 * **Amplitude** - Turns on for as long as the amplitude of the audio signal on Analog Pin 3 is above a certain threshold.
 * **Low-band Amplitude** - Turns on for as long as the amplitude of the audio signal is over a certain threshold in the "low band." The unit is currently running an 8-bit FFT, and I'm taking an average of a slice of it on the low end.
 * **High-band Amplitude** - Turns on for as long as the amplitude of the audio signal is over a certain threshold in the "high band."
 * **Test Clock** - Toggles once per second, this is for testing when no Game Boy is connected.
 * **Null** - Triggers can be disabled with a simple code change by assigning them to the null trigger. If I implement on-the-fly trigger remapping, CPU cycles could be saved by assigning unused triggers to the null trigger at runtime.

### Planned Triggers
I'm currently planning or developing the following triggers:

 * **Mid-band Amplitude**
 *  **MIDI out triggers** - I want to take advantage of the MIDI out functionality in Arduinoboy via a special build of LSDJ that sends MIDI notes from the Game Boy to the Arduino, as this should provide really tight sync with the music.

## Hardware
The circuit layout for Triggerboy is very similar to [the Arduinoboy schematic](http://trash80.com/arduinoboy/arduinoboy_schematic_1_1_0.png), with several changes:

 * The mode button is currently disabled, but Digital Pin 3 is  currently reserved for a button in the future.
 * An amplified mono audio signal may be connected on Analog Pin 3 to drive audio-related triggers. A second audio input is planned for Analog Pin 4 to support stereo sound, but not yet implemented.
 * Only the status LED on Digital Pin 13 is needed. All other LEDs have been removed to make room for more trigger outputs.
 * The MIDI In and MIDI Out components of the circuit have been removed, allowing serial communication with a computer via USB for debugging purposes. Therefore, the TX and RX pins are reserved.
 * All other pins are available to be assigned to various triggers.

## Dependencies
Triggerboy requires the [8-bit Arduino FFT library](http://eliot.s3.amazonaws.com/media/visuals/triggerboy/ArduinoFFT.zip).

It is built and tested against the Arduino Software 1.0.1 and the Arduino UNO, though it may work on older boards.

## Credits
Triggerboy is being developed by myself (Eliot Lash) and [Matt Payne](http://kineticturtle.blogspot.com/). Please direct questions to me.

I'd like to thank Matt for his continued encouragement, making awesome music and visuals, and inspiring me to dive into Arduino development.
I'd also like to thank my dad, [Bob Lash](http://www.bambi.net/bob.html) for mentoring me in electrical engineering.

Triggerboy is based heavily on [Arduinoboy v1.2.3](http://code.google.com/p/arduinoboy/) by Timothy Lamb. It uses the 8-bit version of  fix_fft.c by Tom Roberts et. al., taken from [the Arduino forum](http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1286718155).

## License
Like Arduinoboy, Triggerboy is licensed under the GNU General Public License v2 or later. [See LICENSE file](Triggerboy/blob/master/LICENSE) for full agreement.
