/* SonosIR.ino
 * Version 1.0
 *  
 * This program works like an interface between a Sonos device using ethernet and an amp+remote using IR commands
 * 
 * The Arduino connects to a Sonos device and polls for changes in the play state each second.
 * A Pin defined by IR_SEND_PIN is connected to an IR diode (remember a resistor limiting the current to max 20mA which is the max current from a IO pin)
 * #A - If the Sonos state goes from pause/stop to play, an IR signal is sent to the amp powering it ON and selection the correct input
 * #B - If the Sonos state goes from play to pause/stop, after a delay of 5 minutes, an IR signal is sent ro the amp turning it OFF
 * 
 * There is also an IR reciever connected to a pin defined by IR_RECV_PIN 
 * This IR reciever recieves signals from the remode and has the following functions:
 * #1 - If the input of the amp is changed from the one connected to Sonos the shutdown procedure (#B) is aborted
 * #2 - If the amp remote buttons play or pause/stop is pressed, Sonos playback is started/paused
 * #3 - If the amp remote buttons prev or next is pressed, Sonos goes eiter to previous or next song in the queue
 * 
 * The amp used is a Denon AVR-1804 with the remote RC-940. Codes: http://assets.denon.com/documentmaster/us/denon%20master%20ir%20hex.xls
 * The protocol used here is Sharp. http://www.sbprojects.com/knowledge/ir/sharp.php
 * 
 * Author:
 * Henrik Buestad 12.11.2016
 */

//IRremote library. https://github.com/z3t0/Arduino-IRremote
//To make this work with Denon amps like mine you need my updated ir_Sharp.cpp, IRRemote.h and irRecv.cpp files
#include <IRremote.h>

//SonosUPnP library. https://github.com/tmittet/sonos
#include <SPI.h>
#include <Ethernet.h>
#include <SonosUPnP.h>
#include <MicroXPath_P.h>

//Pins
#define IR_SEND_PIN 3 //this is hard-coded in the IRremote library
#define IR_RECV_PIN 2

//Constants
#define SONOS_STATUS_POLL_DELAY_S 1 //Poll the sonos each second
#define AMP_POWER_OFF_DELAY_S 300 //Wait 300 seconds = 5 minutes before turning off amp
#define ETHERNET_ERROR_DHCP "E: DHCP"
#define ETHERNET_ERROR_CONNECT "E: Connect"

EthernetClient g_ethClient;
SonosUPnP g_sonos = SonosUPnP(g_ethClient, &ethConnectError);

//The MAC address of the Arduino Ethernet shield. Leave as is...
byte g_mac[] = {0x54, 0x48, 0x4F, 0x4D, 0x41, 0x53};
//The IP address of the Arduino Ethernet shield. Should fit inside the range of the Sonos device
IPAddress g_ethernetStaticIP(192, 168, 0, 99);

//The Sonos device IP and ID (can be found in the Sonos app)
IPAddress g_sonosIP(192, 168, 0, 223);
const char g_sonosID[] = "000E582B0312";

IRrecv irrecv(IR_RECV_PIN);
IRsend irsend;
decode_results results;
unsigned long lastSonos = millis();
unsigned long lastIR = millis();

bool cPlay;
int offTimer = -1;

void setup() 
{
  //Serial.begin(9600);
  //delay(10);
  irrecv.enableIRIn(); // Start the receiver
  delay(100);
  if (!Ethernet.begin(g_mac))
  {
    //Serial.println(ETHERNET_ERROR_DHCP);
    Ethernet.begin(g_mac, g_ethernetStaticIP);
  }
  //Serial.println("setup() done...");
}

void loop() 
{
  unsigned long now = millis();
  if((unsigned long)(now - lastSonos) > SONOS_STATUS_POLL_DELAY_MS)
  { 
    pollSonos();
    lastSonos = now; 
  }
  if (irrecv.decode(&results)) 
  {
    if ((unsigned long)(now - lastIR) > 250)
    {
      IRAction(results.decode_type, results.address, results.value);
      //Serial.print(results.decode_type == SHARP ? "SHARP ": "UNKNOWN ");
      //Serial.print(results.address);
      //Serial.print(" ");
      //Serial.println(results.value, HEX); 
    }
    lastIR = now;
    irrecv.resume(); // Receive the next value
  }
}

void IRAction(unsigned int protocol, unsigned int adr, unsigned int data)
{
  if(protocol != SHARP || !(adr == 0x02 || adr == 0x08))
    return;
  switch(data)
  {
    //change source to CD, TV, DVD ...
    case 0xC4: //CD
    case 0xC9: //TV
    case 0xE3: //DVD
      //Serial.println("Changed source");
      if(offTimer >= 0 && cPlay)
      {
        offTimer = -1;
        cPlay = false;
      }
    break;
    case 0x5C: //Play
      //Serial.println("Play");
      g_sonos.play(g_sonosIP);
    break;
    case 0x5E: //Stop
      //Serial.println("Stop/Pause");
      g_sonos.pause(g_sonosIP);
    break;
    case 0x58: //Skip next
      //Serial.println("Skip next");
      g_sonos.skip(g_sonosIP, SONOS_DIRECTION_FORWARD);
    break;
    case 0x59: //Skip prev
      //Serial.println("Skip prev");
      g_sonos.skip(g_sonosIP, SONOS_DIRECTION_BACKWARD);
    break;
    default:
      //not recognized...
    break;
  }
}

void pollSonos()
{
  int st = g_sonos.getState(g_sonosIP); //play  
  bool pl = isPlay(st);  

  //Serial.print("pl = ");
  //Serial.print(pl);
  //Serial.print(" cPlay = ");
  //Serial.print(cPlay);
  //Serial.print(" timer = ");
  //Serial.println(offTimer);

  if(pl && !cPlay)
  {
    //Change state from stop to play 
    //Serial.println("Amp ON!");
    cPlay = true;
    //Denon Sharp Device 2: Power ON: adress 02 = 0x02, data 225 = 0xE1
    irsend.sendSharp(0x02,0xE1); 
    
    delay(5000); //The amp takes some seconds to turn on before beeing able to set source...
    
    //Denon Sharp Device 2: CDR/TAPE: adress 02 = 0x02, data 210 = 0xD2
    irsend.sendSharp(0x02,0xD2); 
    irrecv.enableIRIn(); // Start the receiver over again. It "dies" after a send...
  }
  else if(!pl && cPlay && offTimer == -1)
  {
    // Serial.println("Schedule Amp OFF...");
    //Change state from play to stop, but wait...
    offTimer = AMP_POWER_OFF_DELAY_S;
  }
  if(pl && offTimer != -1)
  {
    offTimer = -1;
  }
  if(offTimer > 0 && cPlay)
  {
    offTimer --;//=(SONOS_STATUS_POLL_DELAY_MS / 1000.0);
  }
  if(offTimer == 0 && cPlay)
  {
    // Serial.println("Amp OFF!");
    cPlay = false;
    offTimer == -1;
    
    //Denon Sharp Device 2: Power OFF: adress 02 = 0x02, data 226 = 0xE2
    irsend.sendSharp(0x02,0xE2); 
    irrecv.enableIRIn(); // Start the receiver over again. It "dies" after a send...
  }
  //Serial.println("pollSonos() done...");
}

bool isPlay(int st)
{
   if(st == 1)
     return true;
   return false; 
}

void ethConnectError()
{
  //Serial.println(ETHERNET_ERROR_CONNECT);
}

