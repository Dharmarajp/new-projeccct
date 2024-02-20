//THIS EXAMPLE SHOWS HOW VVM501 ESP32 4G LTE MODULE CAN CONNECT TO MQTT PUBLIC BROKER HIVEMQ USING TINYGSMCLIENT AND PUBSUBCLIENT LIBRARY
//THE DEVICE CAN PUBLISH AS WELL AS SUBSCRIBE TOPICS VIA 4G MQTT
//FOR VVM501 PRODUCT DETAILS VISIT www.vv-mobility.com

#define TINY_GSM_MODEM_SIM7600  // SIM7600 AT instruction is compatible with A7670
#include<Wire.h> // include I2c library
#include "SSD1306.h" // SSD1306 library


#define SerialAT Serial1
#define SerialMon Serial
#define TINY_GSM_USE_GPRS true
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define RXD2 27    //VVM501 MODULE RXD INTERNALLY CONNECTED
#define TXD2 26    //VVM501 MODULE TXD INTERNALLY CONNECTED
#define powerPin 4 ////VVM501 MODULE ESP32 PIN D4 CONNECTED TO POWER PIN OF A7670C CHIPSET, INTERNALLY CONNECTED

SSD1306 oled(0x3c, 21, 22);    //  0x3c is i2c address,  21 ,22 is SDA, SCL


int LED_BUILTIN = 2;
int ledStatus = LOW;

const char *broker         = "demo.thingsboard.io"; // REPLACE IF YOU ARE USING ANOTHER BROKER
const char *led_on_off     = "v1/devices/me/rpc/request/+";         //SUBSCRIBE TOPIC TO SWITCH ON LED WHENEVER THERE IS INCOMING MESSAGE FROM MQTT. REPLACE XXXXX WITH YOUR TOPIC NAME
const char *message        = "v1/devices/me/telemetry";     //PUBLISH TOPIC TO SEND MESSAGE EVERY 3 SECONDS. REPLACE XXXXX WITH YOUR TOPIC NAME

const char apn[]      = "jionet"; //APN automatically detects for 4G SIM, NO NEED TO ENTER, KEEP IT BLANK

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif
TinyGsmClient client(modem);
PubSubClient  mqtt(client);

unsigned int RLYoffcount =0;
volatile int led_state=0;
volatile int csq_backup;
volatile int led_backup;
int mqtt_retrycnt=0;

char paload_string[10];

void setup()
{
Serial.begin(115200);
pinMode(LED_BUILTIN, OUTPUT);

oled.init();
oled.flipScreenVertically();
oled.setFont(ArialMT_Plain_10);
oled.display();

pinMode(powerPin, OUTPUT);
digitalWrite(powerPin, LOW);
delay(100);
digitalWrite(powerPin, HIGH);
delay(1000);
digitalWrite(powerPin, LOW);
SerialAT.begin(115200, SERIAL_8N1, RXD2, TXD2);


 oled.drawString(10,10,"Initializing modem");  // print humidity on oled 
   oled.display();
  // // Restart takes quite some time
  // To skip it, call init() instead of restart()
  DBG("Initializing modem...");
  if (!modem.init()) {
    DBG("Failed to restart modem, delaying 10s and retrying");
   // return;
  }
  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  DBG("Initializing modem...");
  if (!modem.restart()) {
    DBG("Failed to restart modem, delaying 10s and retrying");
   // return;
  }

  String name = modem.getModemName();
  DBG("Modem Name:", name);

  String modemInfo = modem.getModemInfo();
  DBG("Modem Info:", modemInfo);


  oled.clear();
 oled.drawString(10,10,"Waiting for network..."); 
  oled.display();

  Serial.println("Waiting for network...");
  if (!modem.waitForNetwork()) {
    Serial.println(" fail");
    delay(10000);
   // return;
  }
  Serial.println(" success");

  if (modem.isNetworkConnected()) {
    Serial.println("Network connected");
  }


  // GPRS connection parameters are usually set after network registration
  Serial.print(F("Connecting to "));
  Serial.print(apn);
  if (!modem.gprsConnect(apn)) {
    Serial.println(" fail");
    delay(10000);
   // return;
  }
  Serial.println(" success");

  if (modem.isGprsConnected()) {
    Serial.println("LTE module connected");

  oled.clear();
 oled.drawString(10,10,"LTE module connected"); 
  oled.display();
  }


  // MQTT Broker setup
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(callback);
}

long t1 = 0;
int csq=0;

void loop()
{

   if (millis() - t1 > 20000)
    {
      t1=millis();
       csq = modem.getSignalQuality();
      DBG("Signal quality:", csq);
    }



    if(csq!=csq_backup || led_state!=led_backup){
      led_backup = led_state;
      csq_backup =csq;
    oled.clear();
    oled.setFont(ArialMT_Plain_16);
    oled.drawString(20,10,"CSQ: " + String(csq));  // print humidity on oled 
    if(led_state==1)
    oled.drawString(20,30,"LED: ON");  // print humidity on oled 
    else
        oled.drawString(20,30,"LED: OFF");  // print humidity on oled 
    oled.display();
    }
  

  if (!mqtt.connected()) {
    reconnect();                //Just incase we get disconnected from MQTT server
  }

  if (!modem.isNetworkConnected()) {
    Serial.println("Network disconnected");
    if (!modem.waitForNetwork(180000L, true)) {
      Serial.println(" fail");
      delay(10000);

    }
    if (modem.isNetworkConnected()) {
      Serial.println("Network re-connected");

    }

#if TINY_GSM_USE_GPRS
    // and make sure GPRS/EPS is still connected
    if (!modem.isGprsConnected()) {
      Serial.println("GPRS disconnected!");
      Serial.print(F("Connecting to "));
      Serial.print(apn);
      if (!modem.gprsConnect(apn)) {
        Serial.println(" fail");
        delay(10000);

      }
      if (modem.isGprsConnected()) {
        Serial.println("GPRS reconnected");

      }
    }
#endif
  }

  mqtt.loop();
  delay(10);
}



void reconnect() {
  while (!mqtt.connected()) {       // Loop until connected to MQTT server
    Serial.print("Attempting MQTT connection...");


    // Or, if you want to authenticate MQTT:
     boolean status = mqtt.connect("b0bAnZV8IVM1cLKJmMjp", "b0bAnZV8IVM1cLKJmMjp", "");

    if (status == false) {
      mqtt_retrycnt++;
      Serial.println(" fail");
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);  // Will attempt connection again in 5 seconds
      //        return false;
        oled.clear();
        oled.setFont(ArialMT_Plain_10);
 oled.drawString(10,10,"connecting thingsboard....."); 
  oled.display();
if(mqtt_retrycnt>=5)
{
  mqtt_retrycnt=0;
  setup();
}

    }

    else {

      Serial.println(" success");
      mqtt.subscribe(led_on_off);       //Subscribe to Learning Mode Topic

    }
  }
}


void callback(char* topic, byte* payload, unsigned int length) {                    //The MQTT callback which listens for incoming messages on the subscribed topics

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(json);

 StaticJsonDocument<200> doc;
DeserializationError error  = deserializeJson(doc, json);
if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
  //  return;
}

  String methodName = String((const char*)doc["method"]);


if (doc["params"] == "on") {
    Serial.println("on read");
    strncpy(paload_string,"on",3);
    digitalWrite(LED_BUILTIN, 1);
    led_state=1;

}
else if(doc["params"] == "off"){
    strncpy(paload_string,"off",4);
    digitalWrite(LED_BUILTIN, 0);
    led_state=0;
}
String _payload = "{";
  _payload += "\"RLYoffcount\":"; _payload += paload_string; 
  _payload += "}";
  Serial.println(_payload);
  mqtt.publish("v1/devices/me/telemetry",_payload.c_str());
}

//******************************************************************************
