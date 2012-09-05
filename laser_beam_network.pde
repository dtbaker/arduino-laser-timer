
#include <SPI.h>
#include <Ethernet.h>


byte mac[] = {  0x1E, 0xAD, 0xBE, 0xEF, 0xEE, 0xED };
byte ip[] = { 192,168,0,175 };
//byte server[] = { 203,191,38,244 }; // dtbaker.com.au
byte server[] = { 192,168,0,6 }; // laptop
byte gateway[] = { 192,168,0,1 };
byte subnet[] = { 255, 255, 255, 0 };


int photocellPin = 1;
int photocellReading;  
int LEDpin = 4;
int Buzzpin = 5;
int highvalue = 0;
int lowvalue = 10000;
int cutoff = 200;
int x;
boolean enableSerial = true;

void setup() {
  Ethernet.begin(mac, ip, gateway, subnet);
  Serial.begin(9600);
  delay(1000); // ethernet
  pinMode(LEDpin, OUTPUT);     
  stabalise();
}

void stabalise(){
  
  
  if(enableSerial)Serial.println("Stabalising");
  
  long endtime;
  
  for(x = 0;x<5;x++){
    digitalWrite(LEDpin,HIGH);
    delay(100);
    digitalWrite(LEDpin,LOW);
    delay(100);
  }
  
  
  endtime=millis()+5000;
   while(millis() < endtime){
     photocellReading = analogRead(photocellPin);
     if(photocellReading>highvalue){
       highvalue = photocellReading;
     }
     if(photocellReading<lowvalue){
       lowvalue = photocellReading;
     }
   }
   
  for(x = 0;x<5;x++){
    digitalWrite(LEDpin,HIGH);
    delay(100);
    digitalWrite(LEDpin,LOW);
    delay(100);
  }
  
  if(enableSerial)Serial.print("High reading is: ");
  if(enableSerial)Serial.println(highvalue);
  if(enableSerial)Serial.print("Low reading is: ");
  if(enableSerial)Serial.println(lowvalue);
  
  // work out a cutoff value somehow?
  cutoff = ((highvalue - lowvalue) / 2) + lowvalue;
}

void cutdetected(){
  
    digitalWrite(LEDpin,HIGH);
   if(enableSerial)Serial.println("Cut!");
 
   for(x = 0;x<3;x++){
     digitalWrite(Buzzpin,HIGH);
     delay(50);
     digitalWrite(Buzzpin,LOW);
     delay(50);
   }
   
   if(enableSerial)Serial.println("Connecting via http");
Client client(server, 80);
   if (client.connect()) {
  delay(1000); // ethernet
      if(enableSerial)Serial.println("connected");
      // Make a HTTP request:
      client.println("GET /timer/stopwatch.php?cut HTTP/1.0");
      client.println("Host: dtbaker.com.au");
      client.println();
  delay(1000); // ethernet
      if (client.available()) {
        char c = client.read();
        if(enableSerial)Serial.print(c);
      }

      // if the server's disconnected, stop the client:
      if (!client.connected()) {
        if(enableSerial)Serial.println();
        if(enableSerial)Serial.println("disconnecting.");
        client.stop();
      }
     digitalWrite(Buzzpin,HIGH);
     delay(1000);
  } 
  else {
    // kf you didn't get a connection to the server:
    if(enableSerial)Serial.println("connection failed");
    digitalWrite(Buzzpin,HIGH);
     delay(2000);
     digitalWrite(Buzzpin,LOW);
     delay(300);
    digitalWrite(Buzzpin,HIGH);
     delay(2000);
     digitalWrite(Buzzpin,LOW);
  }
 
    digitalWrite(LEDpin,LOW);
   
}

void loop(){
  
  photocellReading = analogRead(photocellPin);  
 
 if(photocellReading < cutoff){
   cutdetected();
 }
  //if(enableSerial)Serial.print("Analog reading = ");
  //if(enableSerial)Serial.println(photocellReading);     // the raw analog reading
  //delay(50);
  
  // LED gets brighter the darker it is at the sensor
  // that means we have to -invert- the reading from 0-1023 back to 1023-0
  //photocellReading = 1023 - photocellReading;
  //now we have to map 0-1023 to 0-255 since thats the range analogWrite uses
  //LEDbrightness = map(photocellReading, 0, 1023, 0, 255);
  //analogWrite(LEDpin, LEDbrightness);
  
}

