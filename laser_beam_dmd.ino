
#include <SPI.h>
#include <Ethernet.h>

// config:
#define NETWORKUSEUDP_no
#define ENABLESERIAL false
#define PINGHOME 30 // ping home every X seconds - hmm maybe only when program is DISPLAY, not during LASER
#define LASER_THRESHOLD 1000 // doesn't detect hits 1 second or more apart
long last_laser_detection = 0; // used together with threshold
String server_settings = "program=3&laser_mode=4&laser_mode_laps_max=4&"; // string that will come from server with config.
#define USEDHCPd
// pins
#define PHOTOCELLPIN A1
#define LASERPIN 2 //todo: a pin to control the laser pointer on or off
#define LEDPIN 3
#define BUZZPIN 5 //normally 5, change to 1 if it gets annoying :)
// different program modes:
#define PROGRAM_LASER 1
#define PROGRAM_DISPLAY 2
#define PROGRAM_LASER_AND_DISPLAY 3
// different laser modes
#define LASER_MODE_HOLD 1
#define LASER_MODE_START 2
#define LASER_MODE_END 3
#define LASER_MODE_LAPS 4
int laser_mode_current = 1;
int laser_mode_laps_count = 0;
int laser_mode_laps_max = 6; // allow 6 laps: start trip, then lap 1, 2, 3, 4, 5, finish/stop counting. (7 laser cuts total)

// dmd stuff (may not be used)
#include <DMD.h>
#include <TimerOne.h>  
#include "Arial14.h"
#define DMD_REFRESH_EVERY 100
#define DMD_CLEAR_EVERY 100
String currentText;
int da = 1;
int dd = 1;
DMD dmd(da,dd);
void ScanDMD(){ 
  dmd.scanDisplayBySPI();
}


// networking stuff
byte mac[] = {0, 0 ,0 ,0 ,0 ,0};
// todo: use this library to do DHCP and get our gatewayIpAddress() and use that as the server, rather than hardcoding it.
#ifdef NETWORKUSEUDP
#include <EthernetUdp.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008
IPAddress broadcastIP(10,42,43,255);
unsigned int udpPort = 9876;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming/outgoing packet data,
EthernetUDP Udp;
#else
//IPAddress server(10,42,43,1); // laptop server
IPAddress server(192,168,0,54); // laptop server
//IPAddress server(203,191,38,244); // dtbaker
#endif
#ifdef USEDHCP
  // no need for ip addresses, get all that below
#else
  IPAddress us(192,168,0,55); // us, quicker than dhcp for debugging.
  IPAddress gateway(192,168,0,1);
  IPAddress subnet(255, 255, 255, 0);
#endif
int allow_http = true; // changes to false if we get a failed dial home 


// laser / photocell stuff
int photocellReading;  
int highvalue = 0; // laser analog high and low points, set during calibration.
int lowvalue = 10000;
int cutoff = 200;// generated automatically during calibration, 200 is not used


// configuration / counters.
int program_type = PROGRAM_LASER; // 1 = laser cut detection, 2 = dmd display. set by base station when we phone home.
int system_id = 0; // unique identification number of each device. set by base station when we phone home.
int x;
long lastpinghome = 0; // millis() since we contacted home.


void setup() {
  
  randomSeed(analogRead(A1));
  
  pinMode(4,OUTPUT);
   digitalWrite(4,HIGH);
   
  pinMode(LASERPIN, OUTPUT); 
  pinMode(LEDPIN, OUTPUT);     
  pinMode(BUZZPIN, OUTPUT);     
  
  //Ethernet.begin(mac, ip, gateway, subnet);
  if(ENABLESERIAL)Serial.begin(9600);
  if(ENABLESERIAL)Serial.println("Starting up...");
  
  get_mac_address();
  

  #ifdef USEDHCP
  if (Ethernet.begin(mac) == 0) {
    if(ENABLESERIAL)Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    digitalWrite(LEDPIN,HIGH); // report our error by buzzing - hardcore annoying! 
    for(;;)
      ;
  }
  #else
  if(ENABLESERIAL)Serial.println("Setting manual IP address...");  
  Ethernet.begin(mac,us,gateway,subnet);
  #endif
  delay(1000); // ethernet
  
  #ifdef NETWORKUSEUDP
  Udp.begin(udpPort);
  #endif
    
  // print your local IP address from dhcp:
  if(ENABLESERIAL)Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    if(ENABLESERIAL)Serial.print(Ethernet.localIP()[thisByte], DEC);
    if(ENABLESERIAL)Serial.print("."); 
    if(ENABLESERIAL)Serial.println(); 
  }
  
  if(!ping_home("init","init")){
    allow_http=false;
  }
  
  program_type = get_setting("program");
  
   if(ENABLESERIAL){
                String foo = String(program_type);
                Serial.println("Program Type: "+foo);
   }
  
  
  if(program_type == PROGRAM_LASER_AND_DISPLAY){
    display_init();
    laser_init();
  }else if(program_type == PROGRAM_LASER){
    // we have a laser! woo
    laser_init();
  }else if(program_type == PROGRAM_DISPLAY){
    // we have a display! woo
    display_init();
  }
}


/******************* LOOP **********************/
void loop(){
  
  
  if(program_type == PROGRAM_LASER_AND_DISPLAY){
    check_for_cut();
    display_loop();
  }else if(program_type == PROGRAM_LASER){
    check_for_cut();
  }else if(program_type == PROGRAM_DISPLAY){
    // we have a display! woo
    display_loop();
  }
}





/********************************************************************************************
    START SETTINGS / PHONE HOME
*********************************************************************************************/

bool ping_home(String ping_type, String ping_data){
  // report our status to the home pc.
  // our reply from the home pc will contain configuration variables that have been queued for our receival. 
  //return false;
  if(!allow_http)return false;
  
  long pingtime = millis(); // when did we ping?
 
  
  
  #ifdef NETWORKUSEUDP
  if(ENABLESERIAL)Serial.println("Send UDP broadcast message");
     
    /* int data_length = ping_data.length();
  char udpdata[data_length];
  ping_data.toCharArray(udpdata, data_length);*/
  
//  char  ReplyBuffer[] = "acknowledged";       // a string to send back
 
   sprintf (packetBuffer, "time=%d&type=%s&%s", pingtime, ping_type, ping_data);
   
    Udp.beginPacket(broadcastIP, udpPort);
    Udp.write(packetBuffer);
    Udp.endPacket();
    
  #else
   if(ENABLESERIAL)Serial.println("Connecting via http");

    EthernetClient client;    
   if (client.connect(server, 80)) {
      if(ENABLESERIAL)Serial.println("connected");
      // Make a HTTP request:
      String getrequest = "GET /timer/ping.php?";
      getrequest.concat("time=");
      getrequest.concat(pingtime);
      getrequest.concat("&type=");
      getrequest.concat(ping_type);
      getrequest.concat("&");
      getrequest.concat(" HTTP/1.0");
      client.println(getrequest);
    client.println("Host: dtbaker.com.au");
    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
      client.println();
      if (client.available()) {
        char c = client.read();
        if(ENABLESERIAL)Serial.print(c);
      }

      // if the server's disconnected, stop the client:
      if (!client.connected()) {
        if(ENABLESERIAL)Serial.println();
        if(ENABLESERIAL)Serial.println("disconnecting.");
        client.stop();
      }
     //digitalWrite(BUZZPIN,HIGH);
     //delay(1000);
  } 
  else {
    // kf you didn't get a connection to the server:
    if(ENABLESERIAL)Serial.println("connection failed");
    /*digitalWrite(BUZZPIN,HIGH);
     delay(2000);
     digitalWrite(BUZZPIN,LOW);
     delay(300);
    digitalWrite(BUZZPIN,HIGH);
     delay(2000);
     digitalWrite(BUZZPIN,LOW);*/
     return false;
  }
 
 #endif
  
  // read back configuration variables.
  // into named arrays?
  //server_settings = "program=3&displays_across=1&displays_down=1&";
  
  return true;
}
// check for inbound udp message.
void check_inbound(){

  #ifdef NETWORKUSEUDP
  int packetSize = Udp.parsePacket();
  if(packetSize)
  {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remote = Udp.remoteIP();
    for (int i =0; i < 4; i++)
    {
      Serial.print(remote[i], DEC);
      if (i < 3)
      {
        Serial.print(".");
      }
    }
    Serial.print(", port ");
    Serial.println(Udp.remotePort());

    // read the packet into packetBufffer
    Udp.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);
    Serial.println("Contents:");
    Serial.println(packetBuffer);

  }
  #else
  // check for inbound http connection
  #endif

}

int get_setting(String key){
  
  // todo - cache our parsed results so we don't split our string each time we read a setting ? meh.
  
  if(ENABLESERIAL)Serial.println("Checking for setting: "+key);
  if(ENABLESERIAL)Serial.println(" in the string "+server_settings);
    
  int setting = -1; // return value.
  String settingkey,settingval;
  
  char settingschar[server_settings.length() + 1];
  server_settings.toCharArray(settingschar, sizeof(settingschar));
   
  
  char *str1, *str2, *token, *subtoken;
    char *saveptr1, *saveptr2;
    int j;
  for (j = 1, str1 = settingschar; ; j++, str1 = NULL) {
        token = strtok_r(str1, "&", &saveptr1);
        if (token == NULL)
            break;

       for (str2 = token; ; str2 = NULL) {
            subtoken = strtok_r(str2, "=", &saveptr2);
            if (subtoken == NULL)
                break;
            if(setting == -1 && key.equals(settingkey)){
              settingval = String(subtoken);
              char kv[settingval.length() + 1];
              settingval.toCharArray(kv, sizeof(kv));
              setting = atoi(kv);
              if(ENABLESERIAL){
                String foo = String(setting);
                Serial.println(" - FOUND - returning: "+foo);
              }
              return setting;
            }
            settingkey = String(subtoken);
            if(ENABLESERIAL)Serial.println(" - setting: "+settingkey);

        }
    }
    
  return setting;
}

/********************************************************************************************
    END SETTINGS / PHONE HOME
*********************************************************************************************/



/********************************************************************************************
    START DMD  STUFF 
*********************************************************************************************/

void display_init(){
  //da = get_setting("displays_across");
  //dd = get_setting("displays_down");
  if(ENABLESERIAL)Serial.println("Display init..");
  // redoing display:
  //DMD dmd(da, dd);
  dmd.clearScreen( true );
   dmd.selectFont(Arial_14);

  
  Timer1.initialize( 3000 );           //period in microseconds to call ScanDMD. Anything longer than 5000 (5ms) and you can see flicker.
   Timer1.attachInterrupt( ScanDMD );   //attach the Timer1 interrupt to ScanDMD which goes to dmd.scanDisplayBySPI()
   //clear/init the DMD pixels held in RAM

  //clear/init the DMD pixels held in RAM
   
   currentText = "ready...";
}

long timer_start_epoc = 0, timer_end_epoc;
long pausetime = 0;
long last_dmd_update = 0;
long last_dmd_clear = 0;
long resume_timer_display = 0;
char c;
int milisec,sec,mins,first,sec3,mili;
float sec2,secreal,secremain;
bool force_update=false;

int timer_running = 0; // 1 running, 2 finished/paused, 3 hold for a few seconds, 4 reset to 0's
   #define TIMER_STOPPED 0
   #define TIMER_COUNTING 1
   #define TIMER_PAUSED 2 
   #define TIMER_HOLD_DISPLAY 3 // continue counting in background, but hold the display for a short while (for splits)
   #define TIMER_HOLD_DISPLAY_LENGTH 2000
   #define TIMER_RESET 4 // reset our timer to 0 / stopped again.
void start_counter(){
  timer_start_epoc = millis();
  timer_running = TIMER_COUNTING;
}
void pause_counter(){
  timer_running = TIMER_PAUSED;
  pausetime = millis();
}
void hold_counter(){
  timer_running = TIMER_HOLD_DISPLAY;
  resume_timer_display = millis()+TIMER_HOLD_DISPLAY_LENGTH;
}
void resume_counter(){
  timer_start_epoc = timer_start_epoc + (millis()-pausetime);
  timer_running = TIMER_COUNTING;
}
void stop_counter(){ // stop and reset.
  timer_end_epoc = millis();
  timer_running = TIMER_RESET;
}
void draw_current_time(){
  
  long starttime = (millis()-timer_start_epoc);
  
  if(timer_running == TIMER_HOLD_DISPLAY){
    if(millis() < resume_timer_display){
      // draw line along top to say paused.
      dmd.drawLine(  1,  0, 31, 0, GRAPHICS_NORMAL );
      // we still want to display the "held" time while counting in the "background"
      starttime = resume_timer_display - TIMER_HOLD_DISPLAY_LENGTH - timer_start_epoc;
    }else{
      // held time is up, continue counting at correct time.
      timer_running = TIMER_COUNTING;
    }
  }else if(timer_running == TIMER_RESET){
    
    // display the end timer.
    starttime = timer_end_epoc - timer_start_epoc;
    
  }
  
       // timer is counting. increment our counter and display the text.
      milisec = starttime / 100;
      sec2=(float)milisec/10;
      sec = (int)sec2%60;
      // is this more than 1 minute?
      mins = sec2/60;
      /// sec2 is now 1234.40 (seconds.miliseconds)
      //Serial.print(mins);
      //Serial.print(":");
      secreal = sec2 - (int)sec2 + sec;
      //if(secreal<10)Serial.print("0");
      //Serial.println(secreal);
      // draw the minute value (two digits)
      
      
      if(program_type == PROGRAM_LASER_AND_DISPLAY){
          check_for_cut(); // dont return here - keep displaying current tim
        } 
      // todo - put this out in a function so we can call it
      // when a cut is made.
      
      if(mins<10){
        // draw minutes through to miliseconds.
        // minutes:
        c = (char)(((int)'0')+mins);
        dmd.drawChar(  0,  2, c, GRAPHICS_NORMAL );
        dmd.drawChar(  7,  1, ':', GRAPHICS_NORMAL );
        // seconds:
        if(sec<10){
          // draw 0 first
          dmd.drawChar(  9,  2, '0', GRAPHICS_NORMAL );
          c = (char)(((int)'0')+sec);
          dmd.drawChar(  16,  2, c, GRAPHICS_NORMAL );
        }else{
          first = sec/10;
          c = (char)(((int)'0')+first);
          dmd.drawChar(  9,  2, c, GRAPHICS_NORMAL );
          first = first*10;
          c = (char)(((int)'0')+(sec-first));
          dmd.drawChar(  16,  2, c, GRAPHICS_NORMAL );
        }
        
        if(program_type == PROGRAM_LASER_AND_DISPLAY){
          check_for_cut(); // dont return here - keep displaying current tim
        } 
   
        // draw miliseconds.
        dmd.drawChar(  23,  1, '.', GRAPHICS_NORMAL );
        
        sec3 = secreal;
        secremain = secreal - sec3;
        mili = secremain*10;
        c = (char)(((int)'0')+mili);
        dmd.drawChar(  25,  2, c, GRAPHICS_NORMAL );
        
      }else{
        // draw minutes and seconds. no miliseconds because it doesn't fit on single display.
        // todo - adjust to work if multiple displays available.
        first = mins/10;
        c = (char)(((int)'0')+first);
        dmd.drawChar(  0,  2, c, GRAPHICS_NORMAL );
        first = first*10;
        c = (char)(((int)'0')+(mins-first));
        dmd.drawChar(  7,  2, c, GRAPHICS_NORMAL );
        dmd.drawChar(  14,  1, ':', GRAPHICS_NORMAL );
        // and now seconds 
        
        if(sec<10){
          // draw 0 first
          dmd.drawChar(  16,  2, '0', GRAPHICS_NORMAL );
          c = (char)(((int)'0')+sec);
          dmd.drawChar(  24,  2, c, GRAPHICS_NORMAL );
        }else{
          first = sec/10;
          c = (char)(((int)'0')+first);
          dmd.drawChar(  16,  2, c, GRAPHICS_NORMAL );
          first = first*10;
          c = (char)(((int)'0')+(sec-first));
          dmd.drawChar(  24,  2, c, GRAPHICS_NORMAL );
        }
      }
      
      if(timer_running == TIMER_RESET){
          // display the end timer.
          dmd.drawLine(  1,  0, 31, 0, GRAPHICS_NORMAL );
          dmd.drawLine(  1,  15, 31, 15, GRAPHICS_NORMAL );
      }
}
void display_loop(){
  // are we counting a timer or not?
  if(timer_running == TIMER_HOLD_DISPLAY){
    if(millis() - DMD_REFRESH_EVERY > last_dmd_update){
      last_dmd_update = millis();
       force_update=false;
       draw_current_time();
    }
  }else if(timer_running==TIMER_COUNTING){
    if(millis() - DMD_CLEAR_EVERY > last_dmd_clear){
      last_dmd_clear = millis();
      dmd.clearScreen( true );
      force_update=true;//stops flickering when clearing display
    }
    if(force_update || millis() - DMD_REFRESH_EVERY > last_dmd_update){
      last_dmd_update = millis();
       force_update=false;
       draw_current_time();
    }
    //Serial.print(sec);
    //Serial.print(".");
    //Serial.println(sec2);
  }else if(timer_running == TIMER_STOPPED){
    drawTheMarquee(currentText);
  }
}

void drawTheMarquee(String text){
  if(ENABLESERIAL)Serial.println("Drawing marquee: "+text);
   dmd.clearScreen( true );
   dmd.selectFont(Arial_14);
   char marqueeString[text.length()+1];
    text.toCharArray(marqueeString, sizeof(marqueeString));
    dmd.drawMarquee(marqueeString,text.length(),(32)-1,0);
   //dmd.drawMarquee(text,text.length(),(32*DISPLAYS_ACROSS)-1,0);
   long start=millis();
   long timer=start;
   boolean ret=false;
   while(!ret){
     if ((timer+20) < millis()) {
       if(program_type == PROGRAM_LASER_AND_DISPLAY){
        if(check_for_cut())return;
      }
       ret=dmd.stepMarquee(-1,0);
       timer=millis();
       
     }
   }
}


/********************************************************************************************
    END DMD STUFF 
*********************************************************************************************/



/********************************************************************************************
    START LASER POINTER STUFF 
*********************************************************************************************/
void laser_on(){
  digitalWrite(LASERPIN,HIGH);
}
void laser_off(){
  digitalWrite(LASERPIN,LOW);
}
// do this as often as possible!
bool check_for_cut(){
  
    
  photocellReading = analogRead(PHOTOCELLPIN);  
 if(photocellReading < cutoff){
   if(millis() - LASER_THRESHOLD > last_laser_detection){
     last_laser_detection = millis();
      cutdetected();
      return true;
    }
   
 }
 return false;
}
void laser_init(){
  
  if(ENABLESERIAL)Serial.println("Checking laser settings");
  laser_mode_current = get_setting("laser_mode");
  if(laser_mode_current == LASER_MODE_LAPS){
    laser_mode_laps_max = get_setting("laser_mode_laps_max");
  }
  
  if(ENABLESERIAL)Serial.println("Stabalising laser pointer");
  
  laser_on();
  
  long endtime; // how long to stabalise for
  for(x = 0;x<5;x++){
    digitalWrite(LEDPIN,HIGH);
    delay(100);
    digitalWrite(LEDPIN,LOW);
    delay(100);
  }
  
  
  endtime=millis()+5000;
   while(millis() < endtime){
     photocellReading = analogRead(PHOTOCELLPIN);
     if(photocellReading>highvalue){
       highvalue = photocellReading;
     }
     if(photocellReading<lowvalue){
       lowvalue = photocellReading;
     }
   }
   
  for(x = 0;x<5;x++){
    digitalWrite(LEDPIN,HIGH);
    delay(100);
    digitalWrite(LEDPIN,LOW);
    delay(100);
  }
  
  if(ENABLESERIAL)Serial.print("High reading is: ");
  if(ENABLESERIAL)Serial.println(highvalue);
  if(ENABLESERIAL)Serial.print("Low reading is: ");
  if(ENABLESERIAL)Serial.println(lowvalue);
  
  if(highvalue==lowvalue){ // todo - other incorrect values here..
    if(ENABLESERIAL)Serial.println("Configuration failure..");
    digitalWrite(LEDPIN,HIGH);
    highvalue = 100000;
    lowvalue = 0;
    cutoff = 0;
  }else{
  
    // work out a cutoff value somehow?
    cutoff = ((highvalue - lowvalue) / 2) + lowvalue;
  }
  
  String s1 = String(lowvalue);
  String s2 = String(highvalue);
  String s3 = String(cutoff);
  ping_home("laser_init","low=" + s1 + "&high=" + s2 + "&cutoff=" + s3);
  
}

void cutdetected(){
  
  // what do we do here when a cut is detected?
  // first thing we do is report back our cut to the server.
   ping_home("laser_cut","cut=1");
   
  if(program_type == PROGRAM_LASER_AND_DISPLAY){
    
   // the result of the above "ping" will determine what we do
   // we could:
   // - continue running the timer if it's paused / on hold
   // - pause timer if it's running
   // - stop timer and reset to 0
   // - start timer if it's not running
   // - hold timer display on screen, but keep counting behind the scenes.
   
   // what mode are we running
   switch(laser_mode_current){
     case LASER_MODE_LAPS:
     
        if(ENABLESERIAL)Serial.println("Lap mode ");
        if(ENABLESERIAL)Serial.print("current is: ");
        if(ENABLESERIAL)Serial.print(laser_mode_laps_count);
        if(ENABLESERIAL)Serial.print(" of max: ");
        if(ENABLESERIAL)Serial.println(laser_mode_laps_max);
         // we start the timer on the 
         if(laser_mode_laps_count == 0){
           // we haven't started yet! yay
           start_counter();
         }else if(laser_mode_laps_count<laser_mode_laps_max){
           // we just hold the timer
           hold_counter();
         }else if(laser_mode_laps_count>=laser_mode_laps_max){
           // reached the max! stop the timer.
           laser_mode_laps_count = -1; // goes to 0 below.
           stop_counter();
         }
         laser_mode_laps_count++;
       break;
     case LASER_MODE_START:
       if(timer_running == TIMER_STOPPED){
         resume_counter();
       }
       break;
     case LASER_MODE_END:
       if(timer_running == TIMER_COUNTING){
         stop_counter(); // todo - make is show completed time with a border or something?
       }
       break;
     case LASER_MODE_HOLD:
       if(timer_running == TIMER_COUNTING){
          hold_counter();
        }
       break;
     default:
       if(timer_running == TIMER_STOPPED || timer_running == TIMER_PAUSED){
          resume_counter();
        }else if(timer_running == TIMER_COUNTING){
          pause_counter();
        }
       break;
   }
   
    dmd.clearScreen( true );// draw a fresh time on the board.
    draw_current_time();
  }
    digitalWrite(LEDPIN,HIGH);
   if(ENABLESERIAL)Serial.println("Cut!");

   
   // todo- configuration variable to enable/disable buzzer on init.
   for(x = 0;x<1;x++){
     digitalWrite(BUZZPIN,HIGH);
     delay(20);
     digitalWrite(BUZZPIN,LOW);
     delay(20);
   }
   
   
  digitalWrite(LEDPIN,LOW);
   
}
/********************************************************************************************
    END LASER POINTER STUFF 
*********************************************************************************************/

/********************************************************************************************
    START MAC ADDRESS STUFF
*********************************************************************************************/

void get_mac_address ()
{
    generate_random_mac_address();
}
void generate_random_mac_address ()
{
  set_mac_address(random(0, 255), random(0, 255), random(0, 255), random(0, 255), random(0, 255), random(0, 255));
}
void set_mac_address (byte octet0, byte octet1, byte octet2, byte octet3, byte octet4, byte octet5)
{
  mac[0] = octet0;
  mac[1] = octet1;
  mac[2] = octet2;
  mac[3] = octet3;
  mac[4] = octet4;
  mac[5] = octet5;
}

