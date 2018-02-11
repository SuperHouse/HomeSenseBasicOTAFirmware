/*
 * Read from a temperature/humidity sensor and a temperature/barometric pressure sensor
 * and publish the results to an MQTT broker.
 *
 * Uses a DHT12 temperature/humidity sensor and a MS5637-02BA03 pressure/temperature sensor.
 *
 * Dependencies:
 * - BaroSensor library by Angus Gratton (install through Arduino IDE library manager)
 * - DHT12 library by Wemos (download from https://github.com/wemos/WEMOS_DHT12_Arduino_Library )
 *
 * Written by Jonathan Oxer for www.superhouse.tv
 * https://github.com/superhouse/HomeSenseBasicOTAFirmware
 */

/*    YOUR LOCAL CONFIGURATION                          */
#include "config.h"
/* Nothing below this point should require modification */

// Required libraries
#include <ESP8266WiFi.h>   // To allow ESP8266 to make WiFi connection
#include <ESP8266mDNS.h>   // For Arduino OTA updates
#include <WiFiUdp.h>       // For Arduino OTA updates
#include <ArduinoOTA.h>    // For Arduino OTA updates
#include <Wire.h>          // For I2C sensors
#include <BaroSensor.h>    // For Freetronics BARO module www.freetronics.com.au/baro
#include <WEMOS_DHT12.h>   // For Wemos DHT shield wiki.wemos.cc/products:d1_mini_shields:dht_shield
#include <PubSubClient.h>  // MQTT client

// Set the reporting period in seconds
int reporting_interval = REPORTINGINTERVAL;
unsigned long next_report_time = 0;

// WiFi setup
const char* ssid        = WIFISSID;
const char* password    = WIFIPASSWORD;
const char* mqtt_broker = MQTTBROKER;

// Calibration adjustments for sensors
const float humidity_adjustment =       HUMIDITYADJUSTMENT;
const float temperature_adjustment =    TEMPERATUREADJUSTMENT;
const float pressure_adjustment =       PRESSUREADJUSTMENT;
const float temperature_2_adjustment =  TEMPERATURE2ADJUSTMENT;

long lastMsg = 0;
char msg[75];  // General purpose  buffer for MQTT messages

String device_id = "default";
char humidity_topic[50];
char temperature_topic[50];
char pressure_topic[50];
char temperature_2_topic[50];
char command_topic[50];

// Create objects for networking, MQTT, and sensors
WiFiClient espClient;
PubSubClient client(espClient);
DHT12 dht12;

/*
 * Setup
 */
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=====================================");
  Serial.println("Starting up HomeSense Basic OTA v1.0");

  // We need a unique device ID for our MQTT client connection
  device_id = String(ESP.getChipId(), HEX);  // Get the unique ID of the ESP8266 chip in hex
  Serial.print("Device ID: ");
  Serial.println(device_id);

  // Set up the topics for publishing sensor readings. By inserting the unique ID,
  // the result is of the form: "device/d9616f/humidity"
  sprintf(humidity_topic,      "device/%x/humidity",     ESP.getChipId());  // From DHT12
  sprintf(temperature_topic,   "device/%x/temperature",  ESP.getChipId());  // From DHT12
  sprintf(pressure_topic,      "device/%x/pressure",     ESP.getChipId());  // From MS5637-02BA03
  sprintf(temperature_2_topic, "device/%x/temperature2", ESP.getChipId());  // From MS5637-02BA03
  sprintf(command_topic,       "device/%x/command",      ESP.getChipId());  // For receiving messages
  
  // Report the topics to the serial console
  Serial.print("Humidity topic:      ");
  Serial.println(humidity_topic);
  Serial.print("Temperature topic:   ");
  Serial.println(temperature_topic);
  Serial.print("Pressure topic:      ");
  Serial.println(pressure_topic);
  Serial.print("Temperature 2 topic: ");
  Serial.println(temperature_2_topic);
  Serial.print("Command topic:       ");
  Serial.println(command_topic);

  // Bring up the WiFi connection
  setup_wifi();

  // Prepare for OTA updates
  setup_ota();

  // Set up the MQTT client
  client.setServer(mqtt_broker, 1883);
  client.setCallback(callback);

  // Initialise barometric pressure sensor
  BaroSensor.begin();
}

/*
 * Connect to a WiFi network and report the settings
 */
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to SSID ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

/*
 * This callback is invoked when an MQTT message is received. It's not important
 * right now for this project because we don't receive commands via MQTT, so all
 * we do is report any messages to the serial console without acting on them. You
 * can modify this function to make the device act on commands that you send it.
 */
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

/*
 * Repeatedly attempt connection to MQTT broker until we succeed. Or until the heat death of
 * the universe, whichever comes first
 */
void reconnect() {
  char mqtt_client_id[20];
  sprintf(mqtt_client_id, "esp8266-%x", ESP.getChipId());
  
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    //if (client.connect("ESP8266Client-aoeuaoeu")) {
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      sprintf(msg, "Device %s starting up", mqtt_client_id);
      client.publish("events", msg);
      // ... and resubscribe
      client.subscribe(command_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup_ota()
{
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
}

/*
 * =========================================================================
 * Main program loop
 */
void loop() {
  // Handle OTA update requests
  ArduinoOTA.handle();

  // Always try to keep the connection to the MQTT broker alive
  if (!client.connected()) {
    reconnect();
  }

  // Handly any MQTT tasks
  client.loop();

  unsigned long time_now = millis();

  if (time_now >= next_report_time) {
    next_report_time = time_now + (reporting_interval * 1000);

    // Report values from the humidity / temperature sensor
    if (dht12.get() == 0) {
      Serial.print("Temperature:  ");
      float temperature_value = dht12.cTemp + temperature_adjustment;
      Serial.println(temperature_value);
      dtostrf(temperature_value, 4, 2, msg); // This is because printf didn't seem to work in Arduino
      client.publish(temperature_topic, msg);

      Serial.print("Humidity:     ");
      float humidity_value = dht12.humidity + humidity_adjustment;
      Serial.println(humidity_value);
      //printf(msg, "%f", humidityValue);
      dtostrf(humidity_value, 4, 2, msg);
      client.publish(humidity_topic, msg);
    }

    // Report values from the barometric pressure sensor
    if (!BaroSensor.isOK()) {
      Serial.print("Baro sensor not Found/OK. Error: ");
      Serial.println(BaroSensor.getError());
      BaroSensor.begin(); // Try to reinitialise the sensor if we can
    }
    else {
      Serial.print("Pressure:     ");
      float pressure_value = BaroSensor.getPressure() + pressure_adjustment;
      Serial.println(pressure_value);
      dtostrf(pressure_value, 6, 2, msg);
      client.publish(pressure_topic, msg);

      Serial.print("Temperature2: ");
      float temperature_2_value = BaroSensor.getTemperature() + temperature_2_adjustment;
      Serial.println(temperature_2_value);
      dtostrf(temperature_2_value, 4, 2, msg);
      client.publish(temperature_2_topic, msg);
    }
  }
}
