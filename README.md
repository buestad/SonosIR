This program works like an interface between a Sonos device using ethernet and an amp+remote using IR commands

The Arduino connects to a Sonos device and polls for changes in the play state each second.
 A Pin defined by IR_SEND_PIN is connected to an IR diode (remember a resistor limiting the current to max 20mA which is the max current from a IO pin)
 #A - If the Sonos state goes from pause/stop to play, an IR signal is sent to the amp powering it ON and selection the correct input
 #B - If the Sonos state goes from play to pause/stop, after a delay of 5 minutes, an IR signal is sent ro the amp turning it OFF
 
 There is also an IR reciever connected to a pin defined by IR_RECV_PIN 
 This IR reciever recieves signals from the remode and has the following functions:
 #1 - If the input of the amp is changed from the one connected to Sonos the shutdown procedure (#B) is aborted
 #2 - If the amp remote buttons play or pause/stop is pressed, Sonos playback is started/paused
 #3 - If the amp remote buttons prev or next is pressed, Sonos goes eiter to previous or next song in the queue
 
 The amp used is a Denon AVR-1804 with the remote RC-940. Codes: http://assets.denon.com/documentmaster/us/denon%20master%20ir%20hex.xls
 The protocol used here is Sharp. http://www.sbprojects.com/knowledge/ir/sharp.php
 
 
 Henrik Buestad 12.11.2016
 