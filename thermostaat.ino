#include <MemoryFree.h>

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
#define debugSerial3 noDebug
#define debugSerial4 Serial

#define WifiSerial mySerial

char jsonTemp[10];

void setup(void) {
  analogReference(EXTERNAL);
  // initialize inputs/outputs
  // start debugSerial port
  debugSerial.begin(9600);
  debugSerial2.begin(9600);
  debugSerial3.begin(9600);
  debugSerial4.begin(9600);
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
  int j;
  debugSerial2.println("handleIPDMessage");
  // read the rest
  String idlen = WifiSerial.readStringUntil(':');
  int comma = idlen.indexOf(',');
  String id = idlen.substring(0, comma);
  
  debugSerial3.print("Id of incoming is ");
  debugSerial3.println(id);
/*
  // now data length
  int lastchar = idlen.indexOf(':');
  String dataLengthString = idlen.substring(comma+1,lastchar);
  debugSerial2.print("Length is: ");
  debugSerial2.println(dataLengthString);
  //string to int
  /*int dataLength = int(stringToInt(dataLengthString));
  
  debugSerial2.print("Length in integer: ");
  debugSerial2.println(dataLength, DEC);
*/debugSerial3.print("freeMemory()=");
  debugSerial3.println(freeMemory());
  String slash = WifiSerial.readStringUntil('\n');
  debugSerial3.println("slash");
  debugSerial3.println(slash);

  int first = slash.indexOf(' ');
  slash = slash.substring(first+1);
  first = -1;
  int last = slash.indexOf(' ');
  debugSerial3.print("freeMemory()=");
  debugSerial3.println(freeMemory());
  char * url = (char *) malloc(13);
  char * urltemp = (char *) malloc(13);
  sprintf(url, "");
  // in url put substring first+1 to last
  for(j = 0; (j < (last - (first+1))) && j < 12; j++) {
    
    sprintf(urltemp, "%s%c",url,slash.charAt(j+(first+1)));
    sprintf(url, "%s",urltemp);
    
  }

  
  debugSerial3.print("URL: ");
  debugSerial3.println(url);
  /* empty buffer */
  while(WifiSerial.available()) {
    WifiSerial.read();
  }
  debugSerial3.println();
  int gotoUrl = switchURL(url);
  debugSerial3.println(gotoUrl);
  char * sendData = servePage(gotoUrl);
  debugSerial3.println(sendData);
  if(strlen(sendData) != 0) sendToVisitor(id, sendData);
  else debugSerial3.println("Senddata is zero!!");
  free(sendData);
  free(url);
  free(urltemp);
}

void sendToVisitor(String idString, char * pageData) {
  char id[4];
  idString.toCharArray(id,4);
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
  if(!strlen(pageData) == 0) {
    char * cipsend = (char *) malloc(19);
    int len = strlen(pageData);
    if(strcmp(pageData, "frontpage") == 0) len = 642;
    sprintf(cipsend, "AT+CIPSEND=%s,%d",id, len);
    WifiSerial.println(cipsend);
    free(cipsend);
  }
  delay(200);
  while(WifiSerial.available()) {
    debugSerial3.write(WifiSerial.read());
  }
  if(strcmp(pageData, "frontpage") == 0) WifiSerial.print(F("HTTP/1.1 200 OK\nServer: FablabFridge\nConnection: close\nContent-Type: text/html\n\n<html><title>Fablab fridge is cool</title><script>function ajax(){var request=new XMLHttpRequest();request.open('GET', '/get/temp/', true);request.onload=function(){if (request.status >=200 && request.status < 400){var data=JSON.parse(request.responseText); document.getElementById('temp').innerHTML=data.temp;}else{}};request.onerror=function(){};request.send();}</script><body><h1>Fablab de kaasfabriek alkmaar presents:</h1><h2>The fablab fridge</h2>Current temperature is: <span id='temp'></span><br/><button onclick='ajax();'>Get new temperature</button></body></html>"));
  else WifiSerial.print(pageData);
  delay(200);
  
  while(WifiSerial.available()) {
    WifiSerial.read();
  }
  delay(2000);
  char * cipclose = (char *) malloc(16);
  sprintf(cipclose, "AT+CIPCLOSE=%s", id);
  WifiSerial.println(cipclose);
  free(cipclose);
  delay(200);
  while(WifiSerial.available()) {
    debugSerial3.write(WifiSerial.read());
  }
}

char * servePage(int urlNumber) {
  
  char * datatest = (char *) malloc(130);
  char * json;
  int contentLength = 0;
  sprintf(datatest, "HTTP/1.1 200 OK\nServer: FablabFridge\nConnection: close\nContent-Type: text/html\n\n<html><body>%d</body></html>", urlNumber);

  if(urlNumber == 1001) {
    sprintf(datatest, "frontpage");
  }
  if(urlNumber == 1002) {
    json = getTempJsonResponse();   
    contentLength = strlen(json);
  }  
  if(urlNumber == 1003) {
    json = getSuccesJsonResponse(1, "just because");    
    contentLength = strlen(json);  
  }    

 
  if(urlNumber == 1004) {
    json = getSuccesJsonResponse(0, jsonTemp);
    contentLength = strlen(json);
  }      
  if(urlNumber == 1002 || urlNumber == 1003 || urlNumber == 1004) 
    sprintf(datatest, "HTTP/1.1 200 OK\nServer: FablabFridge\nConnection: close\nContent-Type: application/json\n\n%s", json);
    free(json);

  if(urlNumber == 404) {    
    sprintf(datatest, "HTTP/1.1 404 Not Found\nServer: FablabFridge\nConnection: close\nContent-Type: text/html\n\nniet gevonden");
  }      
  
  return datatest;
}
char *ftoa(char *a, double f, int precision)
{
 long p[] = {0,10,100,1000,10000,100000,1000000,10000000,100000000};
 
 char *ret = a;
 long heiltal = (long)f;
 itoa(heiltal, a, 10);
 while (*a != '\0') a++;
 *a++ = '.';
 long desimal = abs((long)((f - heiltal) * p[precision]));
 itoa(desimal, a, 10);
 return ret;
}
char * getTempJsonResponse() {
  
  char * json = (char *) malloc ((11+3+6+1));
  char * temp = (char *) malloc (3+1+2);
  temp = ftoa(temp, temperature, 2);
  sprintf(json, "{ \"temp\": %s }", temp);
  debugSerial3.print("JSON: ");
  debugSerial3.println(json);
  free(temp);
  return json;
}

char * getSuccesJsonResponse(boolean truefalse, char *reason) {
  char * json = (char *) malloc ((30+10+1));
 
  sprintf(json, "{ \"success\": %c, \"reason\": \"%s\" }", ((char)truefalse)+48, reason);
  
  debugSerial3.print("JSON: ");
  debugSerial3.println(json);
  return json;
}

int tryToSetTempTo(float target) {
  if(target > maxtemp || target < mintemp) {
    sprintf(jsonTemp, "minMaxTemp");
    return 1004;
  }
  else {
    targetTemp = target;
    return 1003;
  }
}

int switchURL(char * url) {
  // main page?
  if(strcmp(url, "/") == 0) {    
    return 1001; // start page
  }
  // get action?
  if(strcmp(url, "/get/temp/") == 0) {    
    return 1002; // temp get page
  }
  
  // set action?
  boolean startwith = true;
  int j;
  const char compare[] = "/set/";
  for(j = 0; j < 5 && j < strlen(url); j++) {
    if(!url[j] == compare[j]) startwith = false; 
  }
  if(j >= strlen(url)) startwith = false;
  
  if(startwith) {
    // check if trailing slash
    boolean found = false;
    for(j = 5; j < strlen(url) && found == false; j++) {
      if(url[j] == '/') found = true;
    }
    int check;
    if(found) check = j-1;
    else check = -1;
    
    debugSerial3.println(check, DEC);
    if(check == -1) return 404;
    String targetTempString = String(url).substring(5,check);
    float target = stringToInt(targetTempString);
    debugSerial3.println(target, 2);
    // try to set temp
    int result = tryToSetTempTo(target);
    return result; // succes page 1003 - bad temp = 1004
  }
  

  
  
  return 404;  
}

float stringToInt(String dataLengthString) {
  float result = dataLengthString.toFloat();
  debugSerial4.print("Result: ");
  debugSerial4.println(result, 2);
  /*debugSerial4.print("String: ");
  debugSerial4.println(dataLengthString);
  debugSerial4.print("String length: ");
  debugSerial4.println(dataLengthString.length());
  float dataLength = 0;
  int start = 0;
  boolean decimal = false;
  int decimalPosition = 0;
  if(dataLengthString.charAt(0) == '-') start = 1; // negative
  
  for(int i = start; i < dataLengthString.length(); i++) {
    char character = dataLengthString.charAt(i);
    // point?
    if(character == '.') {
      debugSerial4.println("decimal found");
      decimal = true;
    }
    else {
      if(!decimal) {
        debugSerial4.println("Keer 10");
        dataLength *= 10;
        dataLength += character - 48;
      } else
       {
        debugSerial4.println("Decimal");
        decimalPosition++;
        dataLength += (character - 48) / pow(10, decimalPosition);
      }
    }
    debugSerial4.print("Number is now: ");
    debugSerial4.println(dataLength);
  }
  debugSerial4.println("Keer 10");
  // negative?
  if(start == 1) {
    dataLength *= -1;
    debugSerial4.println("*-1");
  }
  debugSerial4.print("Number is now: ");
  debugSerial4.println(dataLength);
  */
  return result;
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

