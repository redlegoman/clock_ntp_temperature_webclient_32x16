#include "./ht1632c_C.h"
//#include <font_koi8.h>

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Time.h>

#include <DateTime.h>
#include <DateTimeStrings.h>

/* ******** Ethernet Card Settings ******** */
// Set this to your Ethernet Card Mac Address
byte mac[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }; 

/* ******** NTP Server Settings ******** */
//IPAddress timeServer(10,10,10,150);
//193.79.237.14
IPAddress timeServer(193,79,237,14);
/* Set this to the offset (in seconds) to your local time
   This example is GMT - 6 */
//const long timeZoneOffset = -00L;  
//add 2 seconds to time, because there always seems to be a 2seconds difference after ntp sync
const long timeZoneOffset = 2;  

//sync NTP every ntpSyncTime seconds
unsigned int ntpSyncTime = 3600;       
// sync temperature from web site every tempSyncTIme seconds
unsigned int tempSyncTime = 10;         
/* ALTER THESE VARIABLES AT YOUR OWN RISK */
// local port to listen for UDP packets
unsigned int localPort = 8888;
// NTP time stamp is in the first 48 bytes of the message
const int NTP_PACKET_SIZE= 48;      
// Buffer to hold incoming and outgoing packets
byte packetBuffer[NTP_PACKET_SIZE];  
// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;       
// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

// Keeps track of how long ago we updated the NTP server
unsigned long ntpLastUpdate = 0;    
// Keeps track of how long ago the temperature was updated from the web server
unsigned long tempLastUpdate = 0;    

// variable to tell us if the last NTP sync was successful or not
int ntpUpdated=0;


ht1632c ledMatrix = ht1632c(&PORTD, 7, 6, 4, 5, GEOM_32x16, 1);

int FD=0.1;
int WIDTH=32;

char datetime[17] = "01/01/2012 12:00";
char time[6] = "00:00";
String thetime = "11:11";
char tbuf[6];
String seconds = "00";
char secs[3];
//address of webserver
IPAddress webserver(10,10,10,4);
char inString[32]; // string for incoming serial data
int stringPos = 0; // string index counter
boolean startRead = false; // is reading?

void setup() {
  Serial.begin(9600);
//  delay (5000);

  //for(;;);
  
  ledMatrix.clear();
  ledMatrix.pwm(15);
  ledMatrix.setfont(FONT_7x14B);

  // Ethernet shield and NTP setup
  int i = 0;
  int DHCP = 0;
  //      Serial.println("test1");  
  DHCP = Ethernet.begin(mac);
    //  Serial.println("test2");  
  //Try to get dhcp settings 30 times before giving up
  while( DHCP == 0 && i < 5){
    delay(1000);
    DHCP = Ethernet.begin(mac);
    i++;
  }
  if(!DHCP){
    Serial.println("DHCP FAILED");
    ledMatrix.hscrolltext(0,"DHCP FAILED ",RED, 10, 5, LEFT);
    //==========for(;;); //Infinite loop because DHCP Failed
  }else{
  Serial.println("DHCP Success");
  ledMatrix.hscrolltext(0,"DHCP Success ",GREEN, 5, 1, LEFT);
  }
  
  //Try to get the date and time
  int trys=0;
  while(!getTimeAndDate() && trys<10) {
    trys++;
  }

  // print your local IP address:
  Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print("."); 
  }
  Serial.println();

  trys=0;
  while(!getTimeAndDate() && trys<10){
    trys++;
  }
  if(trys<10){
    ntpUpdated=1;
    Serial.println("setup :ntp server update success");
//    ledMatrix.hscrolltext(0,"ntp success ",GREEN, 5, 1, LEFT);
  }else{
    ntpUpdated=0;
    Serial.println("setup:ntp server update failed");
    ledMatrix.hscrolltext(0,"ntp server update failed ",RED, 10, 1, LEFT);
    // here, get time from the RTC instead, then stop checking for NTP unless the network is available again 
    // (ping timeserver?)    
    
  }
  ledMatrix.clear();
  ledMatrix.pwm(15);
  ledMatrix.setfont(FONT_5x7);

}

// Do not alter this function, it is used by the system
int getTimeAndDate() {
   int flag=0;
   Udp.begin(localPort);
   sendNTPpacket(timeServer);
   delay(1000);
   if (Udp.parsePacket()){
     Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer
     unsigned long highWord, lowWord, epoch;
     highWord = word(packetBuffer[40], packetBuffer[41]);
     lowWord = word(packetBuffer[42], packetBuffer[43]);  
     epoch = highWord << 16 | lowWord;
     epoch = epoch - 2208988800 + timeZoneOffset;
     flag=1;
     setTime(epoch);
     ntpLastUpdate = now();
   }
   return flag;
}

// Do not alter this function, it is used by the system
unsigned long sendNTPpacket(IPAddress& address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE); 
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49; 
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;		   
  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket(); 
}


//=========================================================================================================
//boolean touchActive = false;
char dig[6]  = "01234";
char msg[10] = "012345678";
    
int nowHour12 = 0;
int nowHour24 = 0;
int nowMinute = 0;
int nowPM = 0;
   
int dispHour12 = 0;
int dispHour24 = 0;
int dispMinute = 0;
int dispPM = 0;
// initialize colors    
int digit_color = GREEN;
int ampm_color = GREEN;
// initialize array to hold [x, y, prior_x, prior_y] data for touch location
int dot[4] = {0,0,0,0}; 
String pageValue = "00.00";
//===================================================== LOOP ==============================================

void loop() {
  // if there are incoming bytes available 
  // from the server, read them and print them:  

  //Serial.println(pageValue);
  if(now()-tempLastUpdate > tempSyncTime){
    pageValue = connectAndRead();
  }
  // Update the time via NTP server as often as the time you set at the top
  if(now()-ntpLastUpdate > ntpSyncTime) {
    //pageValue = connectAndRead(); //connect to the server and read the output
    int trys=0;
    while(!getTimeAndDate() && trys<2){
      trys++;
    }
  if(trys<2){
    ntpUpdated=1;
    Serial.println("ntp server update success");
//        ledMatrix.hscrolltext(8,"ntp server update success ",GREEN, 10, 1, LEFT);
  }else{
    ntpUpdated=0;
    Serial.println("ntp server update failed");
  //      ledMatrix.hscrolltext(8,"ntp server update failed ",RED, 10, 5, LEFT);
  }
  }
  snprintf(tbuf,sizeof(tbuf), "%02d:%02d", hour(), minute() );
  thetime=tbuf;
  snprintf(secs,sizeof(secs), "%02d", second() );
  seconds=secs;
 
  currentTime();
  setStyle(dispHour12, dispMinute); // set color and brightness based on displayed time
  drawScreen(dispHour12, dispMinute, dispPM); // draw the screen             
  delay(100); // wait for 100ms
      
}//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void currentTime () {
  // set global variables for current time 
  nowHour24 = hour();
        
  if (nowHour24 > 12){
    nowHour12 = nowHour24;
    //  nowHour12 = nowHour24 - 12;
    nowPM = 1;
  }else{
    nowHour12 = nowHour24;
    nowPM = 0;
  }
        
  if (nowHour24 == 12) nowPM = 1;   
  nowMinute = minute();
       
  dispHour24 = nowHour24;
  dispHour12 = nowHour12;
  dispMinute = nowMinute;
  dispPM = nowPM;
}

void setStyle(int hour24, int minute) {
      
        // adjust the brightness based on the time of day
        if (hour24 <= 5) ledMatrix.pwm(0); /// 1 12:00a to 5:59a minimum brightness
        else if (hour24 <= 6) ledMatrix.pwm(5); //5 6:00a to 6:59a brighter
        else if (hour24 <= 19) ledMatrix.pwm(15); //15 7:00a to 7:59p max brightness
        else if (hour24 <= 21) ledMatrix.pwm(5); //5 8:00p to 9:59p dimmer
        else if (hour24 <= 24) ledMatrix.pwm(1); //1  10:00p to 11:59p minimum +1 brightness
        
        // adjust the color based on the time of day
        if (hour24 <= 6) digit_color = RED; // 12:00a to 6:59a red digits
        else if (hour24 <= 19) digit_color = GREEN; // 7:00a to 7:59p green digits 
        else if (hour24 <= 24) digit_color = RED; // 8:00p to 11:59p red digits
        
        ampm_color = ORANGE;

        // adjust the boldness based on the time of day. normal font = less pixels lit = darker for sleeping
        if (hour24 <= 5) ledMatrix.setfont(FONT_7x14B); // 12:00a to 5:59a use a 7x14 normal font
        else if (hour24 <= 21) ledMatrix.setfont(FONT_7x14B); // 6:00a to 9:59p use a bold font for improved distance visibility
        else if (hour24 <= 24) ledMatrix.setfont(FONT_7x14B); // 10:00p to 11:59p use a 7x14 normal font

    }

//void drawScreen( int hour12, int minute, int pm, boolean msgState, char message[10]){    
void drawScreen( int hour12, int minute, int pm){    
        
        char buf[5];
        char ampm = 'A';
        char letter_m = 'M';
        
        dig[0] = '1'; // set flag for valid time
        
        // set the hours digits
        itoa(hour12, buf, 10);
        if (hour12 < 10){
          dig[1] = ' ';
          dig[2] = buf[0];
        }
        else{
          dig[1] = buf[0];
          dig[2] = buf[1];
        }
        // set the minutes digits
        itoa(minute, buf, 10);
        if (minute < 10){
          dig[3] = '0';
          dig[4] = buf[0];
        }
        else{
          dig[3] = buf[0];
          dig[4] = buf[1];
        }
        
        // set the AM/PM state
        if ( pm == 0 ) ampm = 'A';
        else ampm = 'P';
        
        msg[7] = ampm;
        msg[8] = letter_m;                       
      
        
        ledMatrix.putchar(2,-2,dig[1],digit_color,6); // first digit of hour
        ledMatrix.putchar(9,-2,dig[2],digit_color,6); // second digit of hour
        ledMatrix.plot(16,3,ORANGE); // hour:min colon, top
        ledMatrix.plot(16,6,ORANGE); // hour:min colon, bottom
        ledMatrix.putchar(18,-2,dig[3],digit_color,6); // first digit of minute
        ledMatrix.putchar(25,-2,dig[4],digit_color,6); // second digit of minute


/* m                        
 mm#mm   mmm   mmmmm  mmmm  
   #    #"  #  # # #  #" "# 
   #    #""""  # # #  #   # 
   "mm  "#mm"  # # #  ##m#" 
                      #     
                      "                 */
        //temperature
        byte len3 = pageValue.length();
//        Serial.print("len3 = ");
 //       Serial.println(len3);
        ledMatrix.setfont(FONT_5x7);
        const String blank = " ";
        if(len3 == 4){
          ledMatrix.putchar2(0,10,pageValue[0],ORANGE);
          ledMatrix.setfont(FONT_4x6);
          ledMatrix.putchar2(4,11,pageValue[1],ORANGE);
          ledMatrix.setfont(FONT_5x7);
          ledMatrix.putchar2(7,10,pageValue[2],ORANGE);
          ledMatrix.putchar2(11,10,blank[0],ORANGE);
 
        }else{
          ledMatrix.putchar2(0,10,pageValue[0],ORANGE);
          ledMatrix.putchar2(5,10,pageValue[1],ORANGE);
          ledMatrix.setfont(FONT_4x6);
          ledMatrix.putchar2(9,11,pageValue[2],ORANGE);
          ledMatrix.setfont(FONT_5x7);
          ledMatrix.putchar2(11,10,pageValue[3],ORANGE);
        }
        
        ledMatrix.setfont(FONT_4x6);
        byte len2 = strlen(secs);
        ledMatrix.putchar2(25,11,secs[0],RED);
        ledMatrix.putchar2(29,11,secs[1],RED);
                
        // if ntp failed, draw a red dot in the upper left
        if(ntpUpdated==0){
          ledMatrix.plot(0,0,RED);
        }else{
          ledMatrix.plot(0,0,GREEN);
        }  
        
        
        // send the characters to the display, and draw the screen
        ledMatrix.sendframe();
        
    }
//----------------------------------------------------------------------------------------

String connectAndRead(){
  //connect to the server

  Serial.println("connecting to web server...");

  if (client.connect(webserver, 80)) {
    //Serial.println("connected");
    client.println("GET /arduino/temp.php?param=out HTTP/1.0");
//    client.println("GET /arduino/temp.php?param=in HTTP/1.0");

    client.println();

    //Connected - Read the page

    tempLastUpdate = now();
    String temp1 = readPage();
    Serial.print(temp1);Serial.print("|");
    return temp1; //go and read the output

  }else{
    Serial.println("can't connect to webserver");
    return "00.00";
  }

}

String readPage(){
  //read the page, and capture & return everything between '<' and '>'

  stringPos = 0;
  memset( &inString, 0, 32 ); //clear inString memory

  while(true){

    if (client.available()) {
      char c = client.read();

      if (c == '<' ) { //'<' is our begining character
        startRead = true; //Ready to start reading the part 
      }else if(startRead){

        if(c != '>'){ //'>' is our ending character
          inString[stringPos] = c;
          stringPos ++;
        }else{
          //got what we need here! We can disconnect now
          startRead = false;
          client.stop();
          client.flush();
          Serial.println("disconnecting.");
          return inString;

        }

      }
    }

  }
}











