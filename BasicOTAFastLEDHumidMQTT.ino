/*
 * Message format is "position,red,green,blue" like:
 *
 *   3,23,87,12     (Turns on LED 3 with mostly blue tone)
 *   0,255,0,0      (Turns on LED 0 with bright red)
 *   2,0,0,0        (Turns LED 2 totally off)
 *   2,255,255,255  (Turns LED 2 on full white)
 *
 * Position starts at 0
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include "FastLED.h"
#include "DHT.h"

/* ***************************************************** */
/* WiFi settings */
const char* ssid     = "xxxx";  // Put your WiFi SSID here
const char* password = "xxxx";  // Put your WiFi password here

/* FreePixel LED settings */
#define NUM_LEDS 4                // How many leds in your strip?
// For led chips like WS2812, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the WS2801 define both DATA_PIN and CLOCK_PIN
#define DATA_PIN  D2
#define CLOCK_PIN D1
uint32_t ledPosition = 0;
/* RGB values are set in the MQTT callback and then referenced when LED colour is set */
int red   = 0;
int green = 0;
int blue  = 0;
volatile bool updateLights = false;
CRGB leds[NUM_LEDS];    // Define the array of leds


/* Humidity sensor settings */
#define DHTPIN D4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
unsigned long timeLater = 0;


/* MQTT Settings */
String ledTopic   = "/kitchen/statusleds";       // MQTT topic
String humidTopic = "/kitchenfloor/humidity";    // MQTT topic
String tempTopic  = "/kitchenfloor/temperature"; // MQTT topic
IPAddress broker(192,168,1,111);                 // Address of the MQTT broker
#define BUFFER_SIZE 100
#define CLIENT_ID "client-cf3fff"

/* ***************************************************** */
/**
 * MQTT callback to process messages
 */
void callback(const MQTT::Publish& pub)
{
  if (pub.has_stream())
  {
    uint8_t buf[BUFFER_SIZE];
    int read;
    while (read = pub.payload_stream()->read(buf, BUFFER_SIZE))
    {
      Serial.write(buf, read);
    }
    pub.payload_stream()->stop();
    Serial.println("");
  } else
    Serial.print("Message: ");
    Serial.println(pub.payload_string());
    String values = pub.payload_string();
    int c1        = pub.payload_string().indexOf(',');
    int c2        = pub.payload_string().indexOf(',',c1+1);
    int c3        = pub.payload_string().indexOf(',',c2+1);
    ledPosition   = pub.payload_string().toInt();                   // Global
    red           = pub.payload_string().substring(c1+1).toInt();   // Global
    green         = pub.payload_string().substring(c2+1).toInt();   // Global
    blue          = pub.payload_string().substring(c3+1).toInt();   // Global

    updateLights = true;
}

/**
 * Relies on 4 global variables being set: "ledPosition", "red", "green", and "blue"
 */
void setColor()
{
   if (updateLights)
   {
     updateLights = false;
     if(ledPosition <= NUM_LEDS)
     {
       leds[ledPosition].r = red;
       leds[ledPosition].g = green;
       leds[ledPosition].b = blue;
       FastLED.show();
     }
  }
}

WiFiClient wificlient;
PubSubClient client(wificlient, broker);

/**
 * Setup
 */
void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("WiFi begun");
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("Proceeding");

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR   ) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR  ) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR    ) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);

  // Scan red, then green, then blue across the LEDs
  for(int i = 0; i <= NUM_LEDS; i++)
  {
    leds[i] = CRGB::Red;
    FastLED.show();
    delay(200);
    leds[i] = CRGB::Black;
    FastLED.show();
  }
  for(int i = 0; i <= NUM_LEDS; i++)
  {
    leds[i] = CRGB::Green;
    FastLED.show();
    delay(200);
    leds[i] = CRGB::Black;
    FastLED.show();
  }
  for(int i = 0; i <= NUM_LEDS; i++)
  {
    leds[i] = CRGB::Blue;
    FastLED.show();
    delay(200);
    leds[i] = CRGB::Black;
    FastLED.show();
  }
}

/**
 * Main
 */
void loop() {
  ArduinoOTA.handle();
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.begin(ssid, password);

    if (WiFi.waitForConnectResult() != WL_CONNECTED)
      return;
    Serial.println("WiFi connected");
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect(CLIENT_ID)) {  
        client.set_callback(callback);
        client.subscribe(ledTopic);
      }
    }
  }

  unsigned long timeNow = millis();
  if (timeNow >= timeLater) {
    timeLater = timeNow + 60000;

    float humidity    = dht.readHumidity();
    float temperature = dht.readTemperature();

    // Below from stackxchange.com
    char tempC[10];
    dtostrf(temperature,1,2,tempC);
    char relH[10];
    dtostrf(humidity,1,2,relH);

    client.publish(tempTopic, tempC);
    //client.publish("test/humid", temperature);
    client.publish(humidTopic, relH);

    Serial.print("T: ");
    Serial.print(temperature, DEC);
    Serial.print("C H:");
    Serial.print(humidity, DEC);
    Serial.println("%");
  }

  if (client.connected())
    client.loop();
  setColor();
}
