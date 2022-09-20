/*
 *  © 2022 Harald Barth
 *  All rights reserved.
 *  
 *  This is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  It is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this project.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
  This was inspired by works from Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp8266-nodemcu-mqtt-publish-ds18b20-
*/
#define TEMPERATURE
#define VERBOSE_SEND
//#define VERBOSE_OK
//#define VERBOSE_IN

#ifdef TEMPERATURE
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>

/* credentials.h contains defines of WIFI_SSID and WIFI_PASSWORD */
#include "credentials.h"

// Raspberri Pi Mosquitto MQTT Broker
#define MQTT_HOST IPAddress(192, 168, 0, 17)
// For a cloud MQTT broker, type the domain name
//#define MQTT_HOST "example.com"
#define MQTT_PORT 1883

// Electrical meter MQTT Topic
#define MQTT_PUB "tingfast45/electric/meter/"

#ifdef TEMPERATURE

// Temperature MQTT Topics
#define MQTT_PUB_TEMP "tingfast45/elmeterESP/temperature/"

// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;          
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
int numberOfDevices = 0;

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);
// Temperature value
float temp;

// function to print a temperature device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++){
    if (deviceAddress[i] < 16) Serial.print("0");
      Serial.print(deviceAddress[i], HEX);
  }
}

#endif


AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  (void)event;
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  (void)event;
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  (void)reason;
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

/*void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}*/

void onMqttPublish(uint16_t packetId) {
  (void)packetId;
#ifdef VERBOSE_OK
  Serial.print("Pub OK packetId: ");
  Serial.println(packetId);
#endif
}

#ifdef TEMPERATURE
DeviceAddress allDevices[10]; 
#endif

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(500); //in ms, expected time is max 100ms

#ifdef TEMPERATURE
  DeviceAddress tempDeviceAddress; 
  sensors.begin();
  // Init Sensors
  numberOfDevices = sensors.getDeviceCount();
  Serial.println(numberOfDevices);
  int found=0;
  for(int i=0;i<numberOfDevices && found < 10; i++){
    // Search the wire for address
      if(sensors.getAddress(allDevices[found], i)){
	  Serial.print("Found device ");
	  Serial.print(i, DEC);
	  Serial.print(" with address: ");
	  printAddress(allDevices[found]);
	  found++;
	  Serial.println();
      } else {
	  Serial.print("Found ghost device at ");
	  Serial.print(i, DEC);
	  Serial.print(" but could not detect address. Check power and cabling");
      }
  }
  numberOfDevices = found;
#endif
  
  Serial.begin(115200);
  Serial.setTimeout(500); //in ms, expected time is max 100ms

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  //mqttClient.onSubscribe(onMqttSubscribe);
  //mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  // If your broker requires authentication (username and password), set them below
  //mqttClient.setCredentials("REPlACE_WITH_YOUR_USER", "REPLACE_WITH_YOUR_PASSWORD");
  
  connectToWifi();

  Serial.println();
  Serial.println("Version 1.1.0");
  Serial.println("GPLv3");
}
#ifdef TEMPERATURE
void temploop() {
  uint16_t packetIdPub1 = 0;
  sensors.requestTemperatures(); 
  for(int i=0; i<numberOfDevices; i++){
    // Temperature in Celsius degrees
    temp = sensors.getTempC(allDevices[i]);
    String str = String(MQTT_PUB_TEMP) + String(i);
    packetIdPub1 = mqttClient.publish(str.c_str(), 1, true, String(temp).c_str());
    Serial.printf("Pub topic %s packetId: %i val %.2f\n", str.c_str(), packetIdPub1, temp);
  }
}
#endif

void publish(char *t, char *v) {
  uint16_t packetIdPub1 = 0;
  String str = String(MQTT_PUB) + String(t);
  packetIdPub1 = mqttClient.publish(str.c_str(), 1, true, v);
#ifdef VERBOSE_SEND
  Serial.printf("Pub topic %s, packetId: %i val %s\n", str.c_str(), packetIdPub1, v);
#endif
}

char inbuf[1024] = { 0 };

void loop() {
  size_t len = 0;
  char *needle;
  char *ident;
  char *value;
  char *s;
  if ((len=Serial.readBytes(inbuf, 1023)) > 0) {
    inbuf[len] = 0;
#ifdef VERBOSE_IN
    Serial.print("len=");
    Serial.print(len);
    Serial.print(" ");
    Serial.write(inbuf,len);
    Serial.println();
#endif
    s = inbuf;
    if ((needle = strstr(inbuf, "0-0:"))) {
      needle[0] = 0;
      s = needle+4;
      while ((needle = strstr(s, "1-0:"))) {
	needle[0] = 0;
	ident = strtok(s,"(");
	value = strtok(NULL,")");
	publish(ident, value);
	s = needle + 4;
      }
      // last value beyond last 1-0:
      ident = strtok(s,"(");
      value = strtok(NULL,")");
      publish(ident, value);
    }
  } else {
    //Serial.println("Timeout");
    delay(20);
  }
  delay(20);
#ifdef TEMPERATURE
  static unsigned long timestamp = 0;
  if(millis() - timestamp > 60000) {
    timestamp = millis();
    temploop();
  }
#endif
}
