#include <DHT.h>  
#define DHTPIN D5

//rtc
#include <Wire.h>
#include <RTClib.h>
RTC_DS1307 rtc;
//

DHT dht(DHTPIN, DHT22);
const int led=D3;
const int relay=D6;

//thingspeak
String apiKey = "GO6AYRHYHMQDJCHI";     
const char* server = "api.thingspeak.com";
unsigned long lastUpload = 0;
//

//WIFI
#include <ESP8266WiFi.h>
const char *ssid =  "Airtel_G";
const char *pass =  "hariappu1201";
//const char *ssid="motoedge50fusion_7056";
//const char *pass="abcdefghi";
WiFiClient client;
//

//Thresholds
const float Hthres=90;
const float Tthres=27;
float humidity=80;
float temp=26;
int mot=0;

unsigned long lastRelayTrigger = 0;
const unsigned long cooldown = 1200000;

unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 10000;

int pumpCyclesThisHour = 0;
unsigned long hourWindowStart = 0;
const int maxCyclesPerHour = 3;    
unsigned long readDHT = 0;

//

//relay
unsigned long relayOnTime = 0;
const long relayDuration = 5000;  // relay stays ON for 5 seconds
bool relayActive = false;
//
bool ledActive = false;

void setup(){
  Serial.begin(9600);
  dht.begin();
  pinMode(relay,OUTPUT);
  pinMode(led,OUTPUT);
  //rtc
  Wire.begin(D2, D1);
  rtc.begin();
  //
  Serial.println("yoooo");
  Serial.println("Connecting to ");
  Serial.println(ssid);
//wifi
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print("."); 
  }
  Serial.println("");
  Serial.println("WiFi connected");  
//
  digitalWrite(relay,LOW);
  digitalWrite(led,LOW);
}

void loop() {
  if (!relayActive && WiFi.status() != WL_CONNECTED && (millis() - lastReconnectAttempt >= reconnectInterval)) {

    WiFi.reconnect();
    lastReconnectAttempt = millis();
  }
  //rtc
  DateTime now = rtc.now();
  int currentHour = now.hour();
  Serial.print("Time: ");
  Serial.print(currentHour);
  Serial.print(":");
  Serial.println(now.minute());
  //
  //hour
  if (millis() - hourWindowStart >= 3600000) {
    pumpCyclesThisHour = 0;
    hourWindowStart = millis();
  }
  //
  //timer conditions
  unsigned long now_ms = millis();
  bool cooldownPassed = (now_ms - lastRelayTrigger) >= cooldown;
  bool underLimit = (pumpCyclesThisHour < maxCyclesPerHour);
  //

  if (millis() - readDHT >= 10000) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      humidity = h;
      temp = t;
    }
    else {
      Serial.println("Failed to read from DHT");
    }
    readDHT=millis();
  }

  if (!relayActive && (humidity < Hthres || temp > Tthres) && cooldownPassed && underLimit){
    digitalWrite(relay, HIGH);
    relayOnTime = millis();  // record when relay turned on
    relayActive = true;
    lastRelayTrigger=millis();
    pumpCyclesThisHour++;
  }

//relay fix
  if (relayActive && (millis() - relayOnTime >= relayDuration)) {
    digitalWrite(relay, LOW);
    relayActive = false;
    mot++;
  }


  if (currentHour >= 6 && currentHour < 22) {
    digitalWrite(led,HIGH);
    ledActive=1;
  } 
  else {
    digitalWrite(led,LOW);
    ledActive=0;
  }

//thingspeak
  if (!relayActive && millis() - lastUpload >= 15000) {
    if (client.connect(server, 80)) {
      String postStr = "api_key=";
      postStr += apiKey;
      postStr += "&field1=";
      postStr += String(humidity);
      postStr += "&field2=";
      postStr += String(temp);
      postStr += "&field3=";
      postStr += String(ledActive ? 1 : 0);   // <-- lamp indicator as 0/1
      postStr += "&field4=";
      postStr += String(mot);
      postStr += "\r\n\r\n";

      client.print("POST /update HTTP/1.1\n");
      client.print("Host: api.thingspeak.com\n");
      client.print("Connection: close\n");
      client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
      client.print("Content-Type: application/x-www-form-urlencoded\n");
      client.print("Content-Length: ");
      client.print(postStr.length());
      client.print("\n\n");
      client.print(postStr);  
      mot=0;
    }
    lastUpload = millis();
    client.stop();
  }
}