#include <Arduino.h>

// ENM deps
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

// application deps
#include <PubSubClient.h>

// sensor defines
#define SENSOR_PIN A0
#define INTERVAL 15000
#define ESP_LED 2

const char* applicationUUID = "336141";
const char* ssid = "resin-hotspot";
const char* password = "resin-hotspot";
const char* mqtt_server = "iot.eclipse.org";
const char* TOPIC = "Wxec0cXgwgC9KwBK/sensors";

int sensorLow = 1023;
int sensorHigh = 0;

char chipId[100];
char topic[100];
char reading[100];

bool new_reading = false;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;

bool reconnect() {
  if (client.connected()) {
    return true;
  }

  Serial.print("Attempting MQTT connection...");

  // Create a random client ID
  String clientId = "ESP8266Client-";
  clientId += String(random(0xffff), HEX);

  // Attempt to connect
  if (client.connect(clientId.c_str())) {
    Serial.println("Connected to MQTT gateway");
    return true;
  }

  Serial.print("failed, rc=");
  Serial.print(client.state());

  return false;
}

void readSensor() {
  //read the input from A0 and store it in a variable
  int sensorValue = analogRead(A0);

  // Check if any reads failed and exit early (to try again).
  if (isnan(sensorValue)) {
    Serial.println("Failed to read from sensor!");
    return;
  }

  dtostrf(sensorValue, 3, 1, reading);
  new_reading = true;
}

void transmit() {
  char payload[88];

  Serial.println("transmitting");

  snprintf(payload, 88, "{\"type\":\"float\",\"value\":%s,\"device\":{\"id\":\"%s\"},\"apiVersion\":\"3.0.0\"}", reading, chipId);

  Serial.print(topic);
  Serial.print(" ");
  Serial.println(payload);

  client.publish(topic, payload);

  new_reading = false;
}

void setup(void) {
  Serial.begin(115200);
  Serial.println();

  // Get the unique chip ID
  itoa(ESP.getChipId(), chipId, 16);
  Serial.print("chip ID: ");
  Serial.println(chipId);

  // Set the per-device sensor topic
  snprintf(topic, 100, "%s/%s", TOPIC, chipId);
  Serial.print("using the following topic to transmit readings: ");
  Serial.println(topic);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ESP_LED, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);

  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(ESP_LED, HIGH); // LOW is ON

  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.hostname(applicationUUID);

  Serial.print("Connecting to gateway...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to gateway");

  // calibrate for the first five seconds after program runs
  Serial.println("Calibrating sensor...");
  while (millis() < 5000) {
    // record the maximum sensor value
    int sensorValue = analogRead(SENSOR_PIN);
    if (sensorValue > sensorHigh) {
      sensorHigh = sensorValue;
    }
    // record the minimum sensor value
    if (sensorValue < sensorLow) {
      sensorLow = sensorValue;
    }
  }

  // turn the LED off, signaling the end of the calibration period
  Serial.println("Calibration complete");
  digitalWrite(LED_BUILTIN, HIGH);

  // set up mqtt stuff
  client.setServer(mqtt_server, 1883);

  httpServer.on("/id", [](){
    httpServer.send(200, "text/plain", applicationUUID);
  });
  httpUpdater.setup(&httpServer);
  httpServer.begin();

  reconnect();
}

void loop(void) {
  if (WiFi.isConnected() == true) {
    digitalWrite(ESP_LED, LOW);
  } else {
    // turn the LED off
    digitalWrite(ESP_LED, HIGH);
  }

  if (new_reading == true) {
    // only try to reconnect when there's a new reading
    if (reconnect()) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      transmit();
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      Serial.println("failed to reconnect");
      new_reading = false;
    }
  }

  long now = millis();
  if (now - lastMsg > INTERVAL) {
    lastMsg = now;
    readSensor();
  }

  httpServer.handleClient();
}

