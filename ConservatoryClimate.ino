//Arduino program for scraping local rainfall from a weatherstation and then measuring conservatory humidity..
//both values uploaded and plotted to historical graphing in thingspeak.
//Realtime humidity readout on an LCD screen in conservatory.. plus controls a mains circuit to switch on dehumidifier to bring
//humidity below desirable threshold.

#include <ThingSpeak.h>
#include <WiFiNINA.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>


//BME280 CONFIGURATION
Adafruit_BME280 bme; // I2C

//LCD CONFIGURATION
LiquidCrystal lcd(13, 12, 11, 10, 9, 8); //RS, EN, DATA4, DATA5, DATA6, DATA7
#define REDLITE 6
#define GREENLITE 5
#define BLUELITE 3
int brightness=255; //set lcd brightness level

//WIFI CONFIGURATION
WiFiSSLClient client; //ssl wifi client for rainfall acquisition.

char rainfallHttp[]= "environment.data.gov.uk"; //rainfall server to scrape from
char ssid[] = "Galleon's Grave Outpost";
char pass[] = "Cr1spyduck"; 

//THINGSPEAK CHANNEL DETAILS
unsigned long myChannelNumber = 904190;
const char * myWriteAPIKey = "3LO0WAUBR28IVAZL";

//RELAY PIN CONFIGURATION
#define RELAY 7

//GLOBAL VARIABLES
bool relayOn=false;
float temp=0;
float pres=0;
float humi=0;
float rainFall=0;


void setup() {
  //**************PIN SETUP****************

  //RGB LCD VALUES
  pinMode(REDLITE, OUTPUT);
  pinMode(GREENLITE, OUTPUT);
  pinMode(BLUELITE, OUTPUT);
    
  //RELAY CONTROL PIN
  pinMode(RELAY, OUTPUT);
 //****************************************
    
  Serial.begin(9600); //initialize serial monitor
    while(!Serial);
    
  lcd.begin(20, 4); //initialize user display
  setBacklight(0,255,0,false); //set initial screen colour to green
  
  ThingSpeak.begin(client, 443);  //Initialize ThingSpeak
  
  lcd.print("Climate meter"); //simple user feedback for powering on
  lcd.setCursor(0,1);
  lcd.print("Powering on..");

  unsigned status = bme.begin();  //initialize BME sensor
  Serial.println(F("BME280 test"));  
    
  if (!status) { // test procedure to detect humidity sensor
      Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("No BME Sensor found.Check Wiring.. ");
      while (1){  //infinite loop with user feedback
        setBacklight(255,0,0,false);
        delay(500);
        setBacklight(128,0,0,false);
        delay(500);
      }
     }      
   else Serial.println("BME unit found.");   
  
  Serial.println();
}


void loop() { 
  Serial.println("\n******************************************************\n");
    if(WiFi.status() != WL_CONNECTED){  // Connect (or reconnect) to WiFi
      connectWifi();
    }
    readValues(); //read three local sensor values
    scrapeJson();  //obtain rainfall reading from http request
    printValues(); //print all four values to both Serial Monitor and LCD screen
    relayOn=switchRelay(); //trigger relay and alert user if humidity value is above threshold
    writeTeamspeak(); //send sensor values to teamspeak for analysis and public display  
}

void connectWifi(){
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  lcd.setCursor(0,2);
  lcd.print("Connecting to SSID:");
  lcd.setCursor(0,3);
  if(strlen(ssid)>20){ //if ssid longer than lcd screen length, trim to avoid overrun
      for (int i=0; i<20; i++){
        lcd.print(ssid[i]);
      }
  }
  else lcd.print(ssid);
  
  while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, pass); // Connect to WPA/2 network.
      Serial.print(".");
      delay(5000);     
      } 
    Serial.println("\nConnected.");
}

void writeTeamspeak(){
  // write to the ThingSpeak channel

    ThingSpeak.setField(1, temp); //write to thingspeak variables ready for writing
    ThingSpeak.setField(2, pres);
    ThingSpeak.setField(3, humi);
    ThingSpeak.setField(4, rainFall);
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey); //write values to server and log status code
  if(x == 200){
    Serial.println("Channel update successful.");
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
  
  Serial.println("Waiting 15 seconds..");
  delay(15000);  //do not exceed maximum 1 thingspeak connection per 15 seconds
  
  }

void printValues() {

    lcd.clear();

    Serial.print("Temperature = ");
    Serial.print(temp);
    Serial.println(" *C");
    lcd.setCursor(0,0);
    lcd.print("Temperature = ");
    lcd.print(temp);
   
    Serial.print("Pressure = ");
    Serial.print(pres);
    Serial.println(" hPa");
    lcd.setCursor(0,1);
    lcd.print("Pressure = ");
    lcd.print(pres);
   
    Serial.print("Humidity = ");
    Serial.print(humi);
    Serial.println(" %");
    lcd.setCursor(0,2);
    lcd.print("Humidity = ");
    lcd.print(humi);
    
    Serial.print("Rainfall = ");
    Serial.print(rainFall);
    Serial.println(" mm");
    lcd.setCursor(0,3);
    lcd.print("Rainfall = ");
    lcd.print(rainFall);
    lcd.print("mm");
    
    Serial.println();
}

void readValues(){
    temp=bme.readTemperature();
    pres=bme.readPressure() / 100.0F;
    humi=bme.readHumidity();
  }

void scrapeJson(){ //retrieves local rainfall meter's value from http GET request.
 Serial.print("Connecting to weather server...");
 
  if (client.connect(rainfallHttp, 443)) {  //starts client connection, checks for connection
    Serial.println("connected");
        
    client.println("GET /flood-monitoring/id/stations/059793/measures.json HTTP/1.1");
    client.println("Host: environment.data.gov.uk");
    client.println("Connection: close");  //close 1.1 persistent connection 
    client.println(); //end of get request
  } else {
    Serial.println("connection failed");
  }
   delay (500);
  
  bool firstValue=true;
  while (client.available()) { //look for one of two known byte combinations of "val" in json data..
  
    
    if (client.read()=='v'&& firstValue==true){ //if its the first example (the value we want)
      //Serial.print("found a v, ");
      
      if (client.read()=='a'){
        //Serial.print("found a v followed by a, ");
        if (client.read()=='l'){
        //Serial.print("found a v followed by a, and l ");
        firstValue=false;
        rainFall=client.parseFloat(); //parse the next float the buffer recieves
        }
      }
    }  
  }
  
  Serial.print("Rainvalue scraped and parsed = ");
  Serial.println(rainFall);
  Serial.println();
  client.stop();
  client.flush();
}  

bool switchRelay(){ //determines if humidity is above a threshold.. if so turn dehumidifier on. Has a deadzone in middle to eliminate "throttling"
  if (humi>55){
    digitalWrite(RELAY, LOW);
    setBacklight(255,0,0,false);
    return true;
    }
  if (humi<50){
    digitalWrite(RELAY, HIGH);
    setBacklight(0,255,0,false);
    return false;
    }
  }

void setBacklight(uint8_t r, uint8_t g, uint8_t b, bool verbose) { //overloading of function to allow output to Serial monitor
 
  r = map(r, 0, 255, 0, brightness);
  g = map(g, 0, 255, 0, brightness);
  b = map(b, 0, 255, 0, brightness);
 
  // common anode so invert!
  r = map(r, 0, 255, 255, 0);
  g = map(g, 0, 255, 255, 0);
  b = map(b, 0, 255, 255, 0);

  if (verbose){ //output debug on Serial monitor
    Serial.print("Red= "); Serial.print(r, DEC);
    Serial.print(",Green= "); Serial.print(g, DEC);
    Serial.print(",Blue= "); Serial.println(b, DEC);
  }
  analogWrite(REDLITE, r);
  analogWrite(GREENLITE, g);
  analogWrite(BLUELITE, b);
}
