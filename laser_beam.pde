
int photocellPin = 1;
int photocellReading;  
int LEDpin = 4;
int Buzzpin = 5;
int highvalue = 0;
int lowvalue = 10000;
int cutoff = 200;
int x;
boolean enableSerial = false;

void setup() {
  Serial.begin(9600);
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
  
   if(enableSerial)Serial.println("Cut!");
 
   for(x = 0;x<3;x++){
     digitalWrite(Buzzpin,HIGH);
     delay(50);
     digitalWrite(Buzzpin,LOW);
     delay(50);
   }
 
   
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

