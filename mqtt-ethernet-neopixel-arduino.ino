
// Required libraries:
#include <SPI.h>
// https://pubsubclient.knolleary.net/api
#include <PubSubClient.h>
// https://www.arduino.cc/en/Reference/Ethernet
#include <Ethernet.h>
// https://github.com/FastLED/FastLED/wiki
#include <FastLED.h>

#include "config.h"

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 168);
IPAddress myDns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

#define LED_PIN 8

#define NUM_LEDS    10
#define CHIPSET     WS2812B
#define DATA_PIN    7
#define COLOR_ORDER GRB

#define DELAY 30

CRGB leds[NUM_LEDS];

const char *lastWillMessage = "0"; // the last will message show clients that we are offline
const int TIMEOUT = 10; // seconds

// Variables ...
int counter = 0;

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;
PubSubClient mqttClient(client);

void setup() {
  Serial.begin(9600);
  Serial.println("Starting ...");
  
  // setup io-pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT); 

  FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  LEDS.setBrightness(2);
  
  ledOff();

  neopixelBar(1);
  setupEthernet();
  neopixelBar(5);
  setupMqtt();
  neopixelBar(10);
}

void setupEthernet() {
  delay(10);
  // start the Ethernet connection:
  Serial.println("Initialize Ethernet with DHCP:");

  // use fix IP reduces code size by approx. 3kB !!!!
  //Ethernet.begin(mac, ip, myDns, gateway, subnet);
  
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      neopixelAlert();
      while (true) {
        delay(100); // do nothing, no point running without Ethernet hardware
      }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
      neopixelAlert();
    }
  } else {
    Serial.print("  DHCP assigned IP ");
    Serial.println(Ethernet.localIP());
  }

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    neopixelAlert();
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
    neopixelAlert();
  }
 
}

void setupMqtt() {
  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(TIMEOUT);
  mqttClient.setSocketTimeout(TIMEOUT);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  Serial.print("Message arrived [");Serial.print(topicStr);Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (topicStr == mqttTopicNeopixelSet) {
    Serial.println("Got neopixel switching command ...");
    String inString = "";
    int x = -1;
    int r = -1;
    int g = -1;
    int b = -1;
    for (int i = 0; i < length; i++) {
      if ((char)payload[i] == ',') {
        if (x == -1 ) {
          x = inString.toInt();
          inString = "";
        } else if (r == -1) { 
          r = inString.toInt();
          inString = "";
        } else if (g == -1) {
          g = inString.toInt();
          inString = "";
        } else {
          Serial.println("RUNTIME::ERROR: That was not expected!!");
        }
      } else if ((char)payload[i] == ';') {
        b = inString.toInt();
        inString = "";
        
        leds[x] = CRGB(r, g, b);
        Serial.print(" ... set led nr. ");Serial.print (x);
        Serial.print(" to ");Serial.print(r);Serial.print(",");Serial.print(g);Serial.print(",");Serial.println(b);
        x = -1;
        r = -1;
        g = -1;
        b = -1;
      } else {
         inString += (char)payload[i];
      }
    }
    FastLED.show();
  } else if (topicStr == mqttTopicNeopixelClear) {
    Serial.println("Got neopixel clear command ...");
    FastLED.clear();
    FastLED.show();
  } else {
    Serial.print("Got unexpected data from topic ");Serial.println(topicStr);
  }
}

void loop() {
  checkEthernet();
  checkMqtt();
  mqttClient.loop();
  if (counter < 10) {
    neoPixelLoop();
    counter++;
  } else if (counter == 10) {
    FastLED.clear();
    FastLED.show();
    counter++;  
  }
  
}

void checkEthernet() {
  // TODO
}

void checkMqtt() {
  if (!mqttClient.connected()) {
    while (!mqttClient.connected()) {
      Serial.print("Attempting to open MQTT connection...");
      // connect with last will (QoS=1, retain=true, ) ...
      if (mqttClient.connect("ESP8266_Client", mqttTopicStatus, 1, true, lastWillMessage)) {
        Serial.println("connected");
        mqttClient.subscribe(mqttTopicLedSet);
        mqttClient.subscribe(mqttTopicNeopixelSet);
        mqttClient.subscribe(mqttTopicNeopixelClear);
        publishString(mqttTopicStatus, "1", true);
        flashLed();
      } else {
        Serial.print("MQTT connection failed, retry count: ");
        Serial.print(mqttClient.state());
        Serial.println(" try again in 5 seconds");
        delay(5000);
      }
    }
  }  
}

void publishString(const char* topic, String valueStr, boolean retain) {
  Serial.print("Publishing ");Serial.print(valueStr);Serial.print(" to topic ");Serial.print(topic);Serial.print(" (");Serial.print(mqttServer);Serial.println(")");
  char charBufValue[valueStr.length() + 1];
  valueStr.toCharArray(charBufValue, valueStr.length() + 1); //packaging up the data to publish to mqtt ...
  mqttClient.publish(topic, charBufValue, retain);
}

void neoPixelLoop() {
  int magic = 240 / NUM_LEDS;
  
  for(int dot = 0; dot < NUM_LEDS-2; dot++) { 
      leds[dot] = CHSV(magic*dot,255,255);
      leds[dot+1] = CHSV(magic*dot,255,255);
      leds[dot+2] = CHSV(magic*dot,255,255);
      FastLED.show();
      // clear this led for the next time around the loop
      leds[dot] = CRGB::Black;
      leds[dot+1] = CRGB::Black;
      leds[dot+2] = CRGB::Black;
      delay(DELAY);
  }

  for(int dot = NUM_LEDS-1 ; dot >= 0; dot--) { 
      leds[dot] = CHSV(magic*dot,255,255);
      FastLED.show();
      // clear this led for the next time around the loop
      leds[dot] = CRGB::Black;
      delay(DELAY);
  }  
}

void neopixelBlink() {
  // Turn the first led red for 1 second
  leds[0] = CRGB::Red; 
  FastLED.show();
  delay(1000);
  
  // Set the first led back to black for 1 second
  leds[0] = CRGB::Black;
  FastLED.show();
  delay(1000);
}

void neopixelBar(int numLedsToLight) {
  // First, clear the existing led values
  FastLED.clear();
  for(int led = 0; led < numLedsToLight; led++) { 
    leds[led] = CRGB::Blue; 
  }
  FastLED.show();
}

void neopixelAlert() {
  // First, clear the existing led values
  FastLED.clear();
  for(int led = 0; led < 10; led++) { 
    leds[led] = CRGB::Red; 
  }
  FastLED.show();
}

// =================================================================================================================================
// Helper methods 
// =================================================================================================================================

void ledOn() {
  digitalWrite(LED_PIN, HIGH);
}

void ledOff() {
  digitalWrite(LED_PIN, LOW);
}

void flashLed() {
  for (int i=0; i < 5; i++){
      ledOn();
      delay(100);
      ledOff();
      delay(100);
   }  
   ledOff();
}
  
