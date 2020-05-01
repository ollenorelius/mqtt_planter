/***************************************************
    Adafruit MQTT Library ESP8266 Example

    Must use ESP8266 Arduino from:
        https://github.com/esp8266/Arduino

    Works great with Adafruit's Huzzah ESP board & Feather
    ----> https://www.adafruit.com/product/2471
    ----> https://www.adafruit.com/products/2821

    Adafruit invests time and resources providing this open source code,
    please support Adafruit and open-source hardware by purchasing
    products from Adafruit!

    Written by Tony DiCola for Adafruit Industries.
    MIT license, all text above must be included in any redistribution
 ****************************************************/
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"



/************************* WiFi Access Point *********************************/

#define WLAN_SSID       "Baby I'm wasted"
#define WLAN_PASS       "alliwannado"

/************************* MQTT Setup *********************************/

#define MQTT_SERVER      "192.168.0.16"
#define MQTT_SERVERPORT  1883                   // use 8883 for SSL
#define MQTT_USERNAME    "olle"
#define MQTT_KEY         "mqtttillmig"

#define PLANT_NAME       "Mystery Chili"

#define SENS_EN          16
#define PUMP_EN          12

#define INVERT_OUTPUT    0

IPAddress raspi(192,168,0,16);

WiFiClient client;

Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_SERVERPORT, MQTT_USERNAME, MQTT_KEY);

Adafruit_MQTT_Publish wetnessPub = Adafruit_MQTT_Publish(&mqtt, "home/plant/" PLANT_NAME "/wetness");

// Setup a feed called 'onoff' for subscribing to changes.
Adafruit_MQTT_Subscribe waterTriggerSub  = Adafruit_MQTT_Subscribe(&mqtt, "home/plant/" PLANT_NAME "/water_trigger");
Adafruit_MQTT_Subscribe waterAmountSub   = Adafruit_MQTT_Subscribe(&mqtt, "home/plant/" PLANT_NAME "/water_amount");
Adafruit_MQTT_Subscribe checkIntervalSub = Adafruit_MQTT_Subscribe(&mqtt, "home/plant/" PLANT_NAME "/check_interval");

int wateringAmount = 1000; // default pump on duration, milliseconds
int wateringLimit = 450;   // default watering limit; when wetness is below this, pump some water
int checkInterval = 120;   // default sense interval, seconds
unsigned long lastCheck = 0;
int pingInterval = 5;      // ping interval to keep connection alive, seconds
unsigned long lastPing = 0;

int wetness = 0;

void MQTT_connect();

void setup() {
    pinMode(SENS_EN, OUTPUT);
    pinMode(PUMP_EN, OUTPUT);
    digitalWrite(PUMP_EN, INVERT_OUTPUT); // If output should be inverted, default value is high, else low
    digitalWrite(SENS_EN, LOW);
    Serial.begin(115200);
    delay(10);

    // Connect to WiFi access point.
    Serial.println(); Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WLAN_SSID);

    

    WiFi.begin(WLAN_SSID, WLAN_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (millis() > lastCheck + checkInterval*1000)
        {
            wetness = checkHumidity();
            waterPlant(wetness);
            lastCheck = millis();
        }
    }
    Serial.println();

    Serial.println("WiFi connected");
    Serial.println("IP address: "); Serial.println(WiFi.localIP());

    // Setup MQTT subscription for onoff feed.
    mqtt.subscribe(&waterTriggerSub);
    mqtt.subscribe(&waterAmountSub);
    mqtt.subscribe(&checkIntervalSub);

    lastCheck = 0;
    Serial.println("Setup done.");
}


void loop() {
    if (millis() > lastCheck + checkInterval*1000)
    {
        Serial.println("entering check");
        MQTT_connect();

        readMQTTData();
        wetness = checkHumidity();
        waterPlant(wetness);
        publishMQTTData();
        lastCheck = millis();
        Serial.println("exiting check");
    }
   

    // ping the server to keep the mqtt connection alive
    // NOT required if you are publishing once every KEEPALIVE seconds
    if (millis() > lastPing + pingInterval*1000)
    {
        if(! mqtt.ping()) 
        {
            mqtt.disconnect();
            MQTT_connect();
        }
        lastPing = millis();
    }
    
}



void MQTT_connect() {
    // Function to connect and reconnect as necessary to the MQTT server.
    // Should be called in the loop function and it will take care if connecting.
    int8_t ret;

    // Stop if already connected.
    if (mqtt.connected()) {
        return;
    }

    Serial.print("Connecting to MQTT... ");

    uint8_t retries = 3;
    while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
             Serial.println(mqtt.connectErrorString(ret));
             Serial.println("Retrying MQTT connection in 5 seconds...");
             mqtt.disconnect();
             delay(5000);  // wait 5 seconds
             retries--;
             if (retries == 0) {
                 // basically die and wait for WDT to reset me
                 while (1);
             }
    }
    Serial.println("MQTT Connected!");
}

int checkHumidity()
{
    digitalWrite(SENS_EN, HIGH);
    delay(10);
    
    int dryness = analogRead(A0);
    digitalWrite(SENS_EN, LOW);
    Serial.print("Got wetness ");
    Serial.print(dryness);
    Serial.println(".");
    return dryness;
}

void waterPlant(int currentDryness)
{
    if (currentDryness < wateringLimit)
    {
        Serial.println("Pump on!");
        digitalWrite(PUMP_EN, !INVERT_OUTPUT);
        delay(wateringAmount);
        digitalWrite(PUMP_EN, INVERT_OUTPUT);
    }
}


void publishMQTTData()
{
    Serial.print(F("\nSending wetness val "));
    Serial.print(wetness);
    Serial.print("...");
    if (! wetnessPub.publish(wetness)) {
        Serial.println(F("Failed"));
    } else {
        Serial.println(F("OK!"));
    }
}

void readMQTTData()
{
    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = mqtt.readSubscription(5000))) {
        if (subscription == &waterTriggerSub) {
            Serial.print(F("Got water trigger: "));
            Serial.println((char *)waterTriggerSub.lastread);

            int receivedWateringLimit = atoi((char *)waterTriggerSub.lastread);
            if (receivedWateringLimit != 0)
            {
                wateringLimit = receivedWateringLimit;
            }
        }
        if (subscription == &waterAmountSub) {
            Serial.print(F("Got wateringamount: "));
            Serial.println((char *)waterAmountSub.lastread);

            int receivedWateringAmount = atoi((char *)waterAmountSub.lastread);
            if (receivedWateringAmount != 0)
            {
                wateringAmount = receivedWateringAmount;
            }
        }
        if (subscription == &checkIntervalSub) {
            Serial.print(F("Got checkinterval: "));
            Serial.println((char *)checkIntervalSub.lastread);

            int receivedCheckInterval = atoi((char *)checkIntervalSub.lastread);
            if (receivedCheckInterval != 0)
            {
                checkInterval = receivedCheckInterval;
            }
        }
    }
}