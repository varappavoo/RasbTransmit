FM Transmitter Suite for The Raspberry PI
=======

Currently nothing is working! This would allow you to transmit FM radio with STEREO and RDS with your Raspberry PI without any modifications (except for putting a small antenna on one of the GPIO pins). More information later!

The goal of the project:

 - a server for The Raspberry PI that would receive raw FM MPX baseband and transmit it via the http://www.icrobotics.co.uk/wiki/index.php/Turning_the_Raspberry_Pi_Into_an_FM_Transmitter hack.
 - a client written in Java that will transmit sound to the PI from a soundcard. The DSP for stereo (possibly RDS) will happen on the PC in the Java application.

 For comments: martintzvetomirov * g mail com