# LED-strip
I created this project in order to control an RGBW-LED strip from OpenHab using MySensors.
It is based on the sketch by @maghac and @DrJeff but I have taken away the Motion Sensor stuff, since I did not need that.

I implemented support for the W-channel as well so colour messages are now 4 words (aabbccdd) in hex format since this is what OpenHab sends.

There is an ALARM mode that slowly fades to reddish-white and back like “I see you”. The RELAX mode can now be programmed to fade between 8 settings, each being a 4 word hex state e.g.

ff000000

00ff0000

0000ff00

000000ff

aa000000

00aa0000

0000aa00

000000aa


These are the messages that can be sent using MySensors API

Message type	Payload		Comment
	
2  V_STATUS	0 : 1		Off: On

3  V_PERCENTAGE	0 - 100		Dimmer value 0 - 100%

41 V_RGBW	90abcdef	Fade to colour RGBW 90 ab cd ef

24 V_VAR1	0 - 2000	Fade speed. Higher number = lower speed

25 V_VAR2	0 : 1 : 2	0=Normal : 1= alarm : 2= relax

26 V_VAR3	naabbccdd	Set row n in the relax matrix to aabbccdd 0 <= n <= 7


The LED-strip is driven on +24V using IRLZ44N MOSFETs
I ordered the PCB from OSHPark. Since the design was done in KiCad I did not have to generate the Gerber files myself but just sent the .kicad_pcb file to them directly.

In the Arduino nano sketch I implemented some input parameter checking as to not receive nasty surprises from strange input.

In the picture of the circuit board the part on the right is just a +24V power unit based on an L200 and the heatsink for that IC.
