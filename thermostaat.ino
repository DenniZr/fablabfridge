class NullSerial : public Stream {
public:
  virtual size_t write(uint8_t) { return (1); }
  virtual int available() { return (0); }
  virtual int read() { return (0); }
  virtual int peek() { return (0); }
  virtual void flush() {}
  void begin(uint8_t) {}
  void begin(unsigned long, uint8_t) {}
  void end() {}

};

#include <stdio.h> 
float temperature = 18.0;
unsigned long lastSwitch;
unsigned long minLoopTime = 1000;
unsigned long timeBetweenSwitches = 10000; // 10 secs
float targetTemp = 23.0;
int relayPin = 4;
int buzzerPin = 2;
boolean switchIs = LOW;

int mintemp = -50;
boolean mintempAlarm;
boolean mintempOnOff;
int maxtemp = 280;
boolean maxtempAlarm;
float threshold = 0.50;

/**
 * PINS:
 * 4 Relay
 * 2 Buzzer;
 * A0 Temp sensor
 * 
 */

#include "LedControlMS.h"

/*
 Now we need a LedControl to work with.
 ***** These pin numbers will probably not work with your hardware *****
 pin 12 is connected to the DataIn 
 pin 11 is connected to the CLK 
 pin 10 is connected to LOAD 
 We have only a single MAX72XX.
 */
#define NBR_MTX 2 
LedControl lc=LedControl(12,11,10, NBR_MTX);
int digitCounter=0;
int lettercounter=0;
int positieCounter=0;
char text[32];
boolean donescrolling = true;

/* temp sensor */
// which analog pin to connect
#define THERMISTORPIN A0         
// resistance at 25 degrees C
#define THERMISTORNOMINAL 100000      
// temp. for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25   
// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 5
// The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT 2994.04
// the value of the 'other' resistor
#define SERIESRESISTOR 100000   
uint16_t samples[NUMSAMPLES];
/* temp sensor */


#include "SoftwareSerial.h"
SoftwareSerial mySerial(6, 7); // RX, TX
NullSerial noDebug;
#define debugSerial noDebug
#define debugSerial2 noDebug
#define debugSerial3 Serial
#define WifiSerial mySerial

String jsonTemp;

void setup(void) {
  analogReference(EXTERNAL);
  // initialize inputs/outputs
  // start debugSerial port
  debugSerial.begin(9600);
  debugSerial2.begin(9600);
  debugSerial3.begin(9600);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, switchIs);
  lastSwitch = millis();
  

  mintempAlarm = false;
  mintempOnOff = false;
  maxtempAlarm = false;
  pinMode(13, OUTPUT);

  // initialize lcd
  digitCounter=0;
  for (int i=0; i< NBR_MTX; i++){
    lc.shutdown(i,false);
  /* Set the brightness to a medium values */
    lc.setIntensity(i,8);
  /* and clear the display */
    lc.clearDisplay(i);
  }
  /*lc.writeString(0,"BOOTING UP");
  delay(1000);*/
  lc.clearAll();
  digitCounter = 0;
  lettercounter = 0;
  positieCounter = 0;
  donescrolling = true;

  /** wifi **/
  setupWifi();
  
}
 
void loop(void) {
  uint8_t i;
  float average;
  
  // take N samples in a row, with a slight delay
  for (i=0; i< NUMSAMPLES; i++) {
  samples[i] = analogRead(THERMISTORPIN);
  delay(10);
  }
  
  // average all the samples out
  average = 0;
  for (i=0; i< NUMSAMPLES; i++) {
   average += samples[i];
  }
  average /= NUMSAMPLES;
  
  debugSerial.print("Average analog reading "); 
  debugSerial.println(average);
  
  // convert the value to resistance
  average = 1023 / average - 1;
  average = SERIESRESISTOR / average;
  debugSerial.print("Thermistor resistance "); 
  debugSerial.println(average);
  
  float steinhart;
  steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C
  
  debugSerial.print("Temperature "); 
  debugSerial.print(steinhart);
  debugSerial.println(" *C");
  temperature = steinhart;

  if(donescrolling)scrollLeft(temperature);
  
    
  
  
  doSwitch();
  
  mintempCheck();
  
  maxtempCheck();
  DoScrollText();
  delay(1000);
  
  while(WifiSerial.available()) {
    searchIPD(WifiSerial.read());
  }
}

void searchIPD(uint8_t input)
{
    debugSerial2.write(input);
    static uint8_t state = 0;
    if (input == '+') { state = 1; }
    else if (state == 1 && input == 'I') { state = 2; }
    else if (state == 2 && input == 'P') { state = 3; }
    else if (state == 3 && input == 'D') { state = 4; }
    else if (state == 4 && input == ',') {
      state = 0;
      handleIPDMessage();  // we received "+IPD,"
    }
    else { state = 0; }
}

void handleIPDMessage() {
  debugSerial2.println("handleIPDMessage");
  // read the rest
  String idlen = WifiSerial.readStringUntil(':');
  int comma = idlen.indexOf(',');
  String id = idlen.substring(0,comma);
  debugSerial3.print("Id of incoming is ");
  debugSerial3.println(id);

  // now data length
  int lastchar = idlen.indexOf(':');
  String dataLengthString = idlen.substring(comma+1,lastchar);
  debugSerial2.print("Length is: ");
  debugSerial2.println(dataLengthString);
  //string to int
  int dataLength = int(stringToInt(dataLengthString));
  
  debugSerial2.print("Length in integer: ");
  debugSerial2.println(dataLength, DEC);

  String slash = WifiSerial.readStringUntil('\n');
  debugSerial3.println(slash);

  int first = slash.indexOf(' ');
  int last = slash.lastIndexOf(' ');
  String url = slash.substring(first+1,last);
  debugSerial3.print("URL: ");
  debugSerial3.println(url);
  /* empty buffer */
  while(WifiSerial.available()) {
    WifiSerial.read();
  }
  debugSerial3.println();
  int gotoUrl = switchURL(url);
  debugSerial3.println(gotoUrl);
  String sendData = servePage(gotoUrl);
  debugSerial3.println(sendData);
  if(sendData.length() != 0) sendToVisitor(id, sendData);
  else debugSerial3.println("Senddata is zero!!");
}

void sendToVisitor(String id, String pageData) {
  delay(200);
  while(WifiSerial.available()) {
    WifiSerial.read();
  }
  delay(20);
  WifiSerial.println("AT");
  delay(20);

  while(WifiSerial.available()) {
    debugSerial3.write(WifiSerial.read());
  }
  debugSerial3.println("pageData: ");
  debugSerial3.println(pageData);
  if(!pageData.length() == 0)WifiSerial.println("AT+CIPSEND=" + id + "," + String(pageData.length(), DEC));
  delay(200);
  while(WifiSerial.available()) {
    debugSerial3.write(WifiSerial.read());
  }
  WifiSerial.print(pageData);
  delay(200);
  
  while(WifiSerial.available()) {
    WifiSerial.read();
  }
  delay(2000);
  WifiSerial.println("AT+CIPCLOSE=" + id);
  delay(200);
  while(WifiSerial.available()) {
    debugSerial3.write(WifiSerial.read());
  }
}

String servePage(int urlNumber) {
  
  String datatest;
  String json;
  int contentLength = 0;
  datatest = "HTTP/1.1 200 OK\n";
    datatest += "Server: FablabFridge\n";
    datatest += "Connection: close\n";
    datatest += "Content-Type: text/html\n";
    datatest += "\n";
    datatest += "<html><body>";
    datatest += String(urlNumber, DEC);
    datatest += "</body></html>";

  if(urlNumber == 1001) {
    
    datatest = "HTTP/1.1 200 OK\n";
    datatest += "Server: FablabFridge\n";
    datatest += "Connection: close\n";
    datatest += "Content-Type: text/html\n";
    datatest += "\n";
    datatest += "<html><body>";
    datatest += "welkom";
    datatest += "</body></html>";
    //datatest = "HTTP/1.1 200 OK\nServer: FablabFridge\nConnection: close\nContent-Type: text/html\n\n<html><title>Fablab fridge is cool</title><script>function ajax(){var request=new XMLHttpRequest();request.open('GET', '/get/temp/', true);request.onload=function(){if (request.status >=200 && request.status < 400){// Success! var data=JSON.parse(request.responseText); document.getElementById('temp').innerHTML=data.temp;}else{// We reached our target server, but it returned an error}};request.onerror=function(){// There was a connection error of some sort};request.send();}</script><body><h1>Fablab de kaasfabriek alkmaar presents:</h1><h2>The fablab fridge</h2>Current temperature is: <span id='temp'></span><br/><button onclick='ajax();'>Get new temperature</button></body></html>";
  }
  if(urlNumber == 1002) {
    json = getTempJsonResponse();   
    contentLength = json.length();
    datatest = "HTTP/1.1 200 OK\n";
    datatest += "Server: FablabFridge\n";
    datatest += "Connection: close\n";
    datatest += "Content-Type: application/json\n";
    datatest += "\n";
    datatest += json;
  }  
  if(urlNumber == 1003) {
    json = getSuccesJsonResponse("true", "just because");    
    contentLength = json.length();
    datatest = "HTTP/1.1 200 OK\n";
    datatest += "Server: FablabFridge\n";
    datatest += "Connection: close\n";
    datatest += "Content-Type: application/json\n";
    datatest += "\n";
    datatest += json;
  }    
  if(urlNumber == 1004) {
    json = getSuccesJsonResponse("false", jsonTemp);
    contentLength = json.length();
    datatest = "HTTP/1.1 200 OK\n";
    datatest += "Server: FablabFridge\n";
    datatest += "Connection: close\n";
    datatest += "Content-Type: application/json\n";
    datatest += "\n";
    datatest += json;
  }      
  if(urlNumber == 404) {    
    datatest = "HTTP/1.1 404 Not Found\n";
    datatest += "Server: FablabFridge\n";
    datatest += "Connection: close\n";
    datatest += "Content-Type: text/html\n";
    datatest += "\n";
    datatest += "niet gevonden";
  }      
  if(datatest.length() == 0){
    debugSerial3.println("Senddata is zero!!");
  
    debugSerial3.print("urlnumber is: ");
    debugSerial3.println(urlNumber);
  }
  return datatest;
}

String getTempJsonResponse() {
  String json = "{ \"temp\": " + String(temperature, 2)  + " }";
  debugSerial3.print("JSON: ");
  debugSerial3.println(json);
  return json;
}

String getSuccesJsonResponse(String truefalse, String reason) {
  String json = "{ \"success\": " + truefalse  + ", \"reason\": \"" + reason + "\" }";
  debugSerial3.print("JSON: ");
  debugSerial3.println(json);
  return json;
}

int tryToSetTempTo(float target) {
  if(target > maxtemp || target < mintemp) {
    jsonTemp = "minMaxTemp";
    return 1004;
  }
  else {
    targetTemp = target;
    return 1003;
  }
}

int switchURL(String url) {
  // main page?
  if(url.equals("/")) {    
    return 1001; // start page
  }
  // get action?
  if(url.startsWith("/get/temp/")) {    
    return 1002; // temp get page
  }
  
  // set action?
  if(url.startsWith("/set/")) {
    // check if trailing slash
    int check = url.indexOf('/',5);
    debugSerial3.println(check, DEC);
    if(check == -1) return 404;
    String targetTempString = url.substring(5,check);
    float target = stringToInt(targetTempString);
    debugSerial3.println(target, 2);
    // try to set temp
    int result = tryToSetTempTo(target);
    return result; // succes page 1003 - bad temp = 1004
  }
  

  
  
  return 404;  
}

float stringToInt(String dataLengthString) {
  float dataLength = 0;
  int start = 0;
  boolean decimal = false;
  int decimalPosition = 0;
  if(dataLengthString.charAt(0) == '-') start = 1; // negative
  
  for(int i = start; i < dataLengthString.length(); i++) {
    int character = dataLengthString.charAt(i);
    // point?
    if(character == '.') decimal = true;
    else {
      if(!decimal) {
        dataLength *= 10;
        dataLength += character - 48;
      }
      if(decimal) {
        decimalPosition++;
        dataLength += (character - 48) / pow(10, decimalPosition);
      }
    }
  }

  // negative?
  if(start == 1) {
    dataLength *= -1;
  }
  return dataLength;
}

void scrollLeft(float ch) {
  dtostrf(ch, 2, 2, text);
  donescrolling = false;  
}

void DoScrollText(){
  debugSerial.println(text);
  lc.clearDisplay(0);

  
  

  debugSerial.print("Lettercounter: ");
  debugSerial.println(lettercounter);
  debugSerial.print("positieCounter: ");
  debugSerial.println(positieCounter);
  int pos1 = lc.getCharArrayPosition(text[lettercounter]);
  int pos2 = lc.getCharArrayPosition(text[lettercounter+1]);
  
  for (int i=0; i<5;i++) {
   lc.setRow(0,i-positieCounter, alphabetBitmap[pos1][i]);
  } 
  for (int i=6; i<11;i++) {
   lc.setRow(0,i-positieCounter, alphabetBitmap[pos2][i-6]);
  } 
  positieCounter++;
   
  if(positieCounter >= 6) {
    lettercounter++;
    positieCounter = 0;
    if(lettercounter == (strlen(text)-1)) {
      lettercounter = 0;      
    }
  }
   if(lettercounter == 0 && positieCounter == 0) {
    donescrolling = true;
   }
  
}

void alarm(int code, byte data[], int index) {
  switch(code) {
    case 3:
    debugSerial.println("Mintemp");    
    mintempAlarm = true;
    tone(buzzerPin, 1031, 400); 
    delay(500);
    tone(buzzerPin, 131, 400); 
    delay(500);
    break;
    case 4:
    debugSerial.println("Maxtemp");    
    maxtempAlarm = true;
    tone(buzzerPin, 1031, 400); 
    delay(500);
    tone(buzzerPin, 531, 400); 
    delay(500);
    break;
  }
}

void watch(int code, byte data[], int index) {
  switch(code) {
    case 0:
    // switch on/off temperature
    debugSerial.println("Switch");
    tone(buzzerPin, 1031, 1000); 
    
    break;
    
  }

}
void doSwitch() {  
  
  if((millis() - lastSwitch) > timeBetweenSwitches) {
    
    if(!mintempAlarm && !maxtempAlarm) {
      
      if(temperature >= (targetTemp - threshold) ) {
        if(switchIs == LOW) {
          switchIs = HIGH;
          digitalWrite(relayPin, switchIs);  
          lastSwitch = millis();      
          watch(0, 0, 0);
          debugSerial.println("Cooling");
        }
      
      } else if (temperature <= (targetTemp + threshold) ) {
        if(switchIs == HIGH) {
          switchIs = LOW;
          digitalWrite(relayPin, switchIs);        
          lastSwitch = millis();
          watch(0, 0, 0);
          debugSerial.println("Cool enough");
        }
      }
    } else {
      if(mintempAlarm || maxtempAlarm) {
       switchIs = LOW; 
       digitalWrite(relayPin, switchIs);
       
        
      }
      
    }
  } else {
    //debugSerial.println("Wait for it");
  }
}

void mintempCheck() {
  if(temperature < mintemp) {
    // mintemp! 
    alarm(3, 0, 0);
  } else {
    mintempAlarm = false;
  }
}
void maxtempCheck() {
  if(temperature > maxtemp) {
    alarm(4, 0, 0);
  } else {
    maxtempAlarm = false;
  }
}
void setupWifi() {
  WifiSerial.begin(9600);
  delay(5000);
  WifiSerial.println("AT+RST");
  delay(5000);
  while(WifiSerial.available()) {
    
    debugSerial2.write(WifiSerial.read());
    
  }
  
  WifiSerial.println("AT");
  delay(200);
  while(WifiSerial.available()) {
    
    debugSerial2.write(WifiSerial.read());
    
  }

  // reset and okay?
  // setup accesspoint

  WifiSerial.println("AT+CWMODE=2");
  delay(200);
  while(WifiSerial.available()) {
    
    debugSerial2.write(WifiSerial.read());
    
  }
  // login details
  WifiSerial.println("AT+CWSAP=\"fablabfridge\",\"fablabfridge\",5,3");
  delay(200);
  while(WifiSerial.available()) {
    
    debugSerial2.write(WifiSerial.read());
    
  }

  // multiple devices
  WifiSerial.println("AT+CIPMUX=1");
  delay(200);
  while(WifiSerial.available()) {
    
    debugSerial2.write(WifiSerial.read());
    
  }
  // open webserver
  WifiSerial.println("AT+CIPSERVER=1,80");
  delay(200);
  while(WifiSerial.available()) {
    
    debugSerial2.write(WifiSerial.read());
    
  }
  
}

