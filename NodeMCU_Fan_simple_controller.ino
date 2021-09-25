// #==================================================================#
// ‖ Author: Luis Alejandro Domínguez Bueno (LADBSoft)                ‖
// ‖ Date: 2021-09-25                                    Version: 1.0 ‖
// #==================================================================#
// ‖ Name: ESP8266 Generic 3-speed fan MQTT controller                ‖
// ‖ Description: A sketch for the NodeMCU ESP8266 microcontroller    ‖
// ‖ board that adds IoT functionality to a generic 3-speed foot or   ‖
// ‖ pedestal fan. The program is design to work with a fan which     ‖
// ‖ motor is controlled by choosing one of 3 coil cables, and NOT    ‖
// ‖ for those whose speeds are chosen by powering one, two or the    ‖
// ‖ three coils at the same time.                                    ‖
// ‖                                                                  ‖
// ‖ The sketch uses 3 pins as inputs, for three toggle buttons (one  ‖
// ‖ for each speed) and 3 pins as outputs, for each of the three     ‖
// ‖ speeds.                                                          ‖
// ‖                                                                  ‖
// ‖ The controllable functions are the speed of the fan and the mode ‖
// ‖ (MANUAL or WI-FI). Both ways of controlling the fan will work at ‖
// ‖ any point of time, but the chosen one will have priority over    ‖
// ‖ the other:                                                       ‖
// ‖ - If MANUAL is selected, the "2" speed button is pushed and a    ‖
// ‖   "3" speed command is received via Wi-Fi, it will be ignored,   ‖
// ‖   and the speed will continue being "2". Wi-Fi will only be      ‖
// ‖   listened to if no buttons are pushed.                          ‖
// ‖ - If WI-FI is selected, the "2" speed button is pushed and a     ‖
// ‖   "3" speed command is received via Wi-Fi, it will be listened   ‖
// ‖   to, and the speed will change to "3".                          ‖
// ‖                                                                  ‖
// ‖ The microcontroller will need to be connected to an MQTT server, ‖
// ‖ for sending status and receiving commands.                       ‖
// #==================================================================#
// ‖ Licensing:                                                       ‖
// #==================================================================#
// ‖ MIT License                                                      ‖
// ‖                                                                  ‖
// ‖ Copyright (c) 2021 ladbsoft                                      ‖
// ‖                                                                  ‖
// ‖ Permission is hereby granted, free of charge, to any person      ‖
// ‖ obtaining a copy of this software and associated documentation   ‖
// ‖ files (the "Software"), to deal in the Software without          ‖
// ‖ restriction, including without limitation the rights to use,     ‖
// ‖ copy, modify, merge, publish, distribute, sublicense, and/or     ‖
// ‖ sell copies of the Software, and to permit persons to whom the   ‖
// ‖ Software is furnished to do so, subject to the following         ‖
// ‖ conditions:                                                      ‖
// ‖                                                                  ‖
// ‖ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,  ‖
// ‖ EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES  ‖
// ‖ OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND         ‖
// ‖ NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT      ‖
// ‖ HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,     ‖
// ‖ WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING     ‖
// ‖ FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR    ‖
// ‖ OTHER DEALINGS IN THE SOFTWARE.                                  ‖
// #==================================================================#
// ‖ Version history:                                                 ‖
// #==================================================================#
// ‖ 1.0:  First stable version released.                             ‖
// #==================================================================#

// +------------------------------------------------------------------+
// |                        I N C L U D E S                           |
// +------------------------------------------------------------------+
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include "Configuration.h"
#include "Commands.h"

// +------------------------------------------------------------------+
// |                         G L O B A L S                            |
// +------------------------------------------------------------------+

WiFiClient espClient;
PubSubClient client(espClient);
long lastCheck = 0;

// +------------------------------------------------------------------+
// |                           S E T U P                              |
// +------------------------------------------------------------------+

void setup() {
  //Disable Serial pins in order to use them as GPIO
  pinMode(1, FUNCTION_3); //TX
  pinMode(3, FUNCTION_3); //RX

  pinMode(IRReceiverPin, INPUT);   // For receiving remote commands
  pinMode(IRSenderPin, OUTPUT);    // For sending commands

  setup_wifi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  commandSetup();
}

// +------------------------------------------------------------------+
// |                            L O O P                               |
// +------------------------------------------------------------------+

void loop() {
  String payload;
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastCheck > 5000) {
    lastCheck = now;

    publishStates();
  }
}

// +------------------------------------------------------------------+
// |                     S U B R O U T I N E S                        |
// +------------------------------------------------------------------+

void setup_wifi() {
  WiFiManager wifiManager;
  wifiManager.setTimeout(180); //3 minutes

  if(!wifiManager.autoConnect(wifiSsid, wifiPassword)) {
    //Retry after 3 minutes with no WiFi connection
    ESP.reset();
    delay(5000);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String topicString = String(topic);
  byte* payloadZeroTerm = (byte*)malloc(length+1);
  String payloadString;
  byte payloadByte;

// Conversion of payload to String
  memcpy(payloadZeroTerm, payload, length);
  payloadZeroTerm[length] = '\0';
  payloadString = String((char*)payloadZeroTerm);

// Conversion of payload to Byte
  payloadByte = (byte)payloadString.toInt();

// Power Topic: Payload will be "ON" or "OFF"
  if(topicString.equals(String(mqttPowerCommandTopic))) {
    if (payloadString.equals("ON")) {
      sendPowerCommand(true);
    } else if (payloadString.equals("OFF")) {
      sendPowerCommand(false);
    }

// Speed Topic: Payload will be "AUTO", "LOW", "MED", or "HIGH"
  } else if(topicString.equals(String(mqttSpeedCommandTopic))) {
    if (payloadString.equals("AUTO")) {
      sendSpeedCommand(0);
    } else if (payloadString.equals("LOW")) {
      sendSpeedCommand(1);
    } else if (payloadString.equals("MED")) {
      sendSpeedCommand(2);
    } else if (payloadString.equals("HIGH")) {
      sendSpeedCommand(3);
    }

// Mode Topic: Payload will be "AUTO", "COOL", "FAN" or "HEAT"
  } else if(topicString.equals(String(mqttModeCommandTopic))) {
    if (payloadString.equals("AUTO")) {
      sendModeCommand(0);
    } else if (payloadString.equals("COOL")) {
      sendModeCommand(1);
    } else if (payloadString.equals("FAN")) {
      sendModeCommand(2);
    } else if (payloadString.equals("HEAT")) {
      sendModeCommand(3);
    }

// Temperature Topic: Payload will be "AUTO" or a number corresponding to the desired temperature, between 17 and 30
  } else if(topicString.equals(String(mqttTempCommandTopic))) {
    if (payloadString.equals("AUTO")) {
      sendTemperatureCommand(0);
    } else {
      if (payloadByte >= 17 && payloadByte <= 30) {
        sendTemperatureCommand(payloadByte);
      }
    }
  }

  publishStates();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    // Attempt to connect
    if (client.connect(mqttClientId, mqttUser, mqttPassword)) {
      // Once connected, resubscribe
      client.subscribe(mqttPowerCommandTopic);
      client.subscribe(mqttModeCommandTopic);
      client.subscribe(mqttTempCommandTopic);
      client.subscribe(mqttSpeedCommandTopic);
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void publishStates() {
//  Publish power state
  if (powerState) {
    client.publish(mqttPowerStateTopic, "ON");
  } else {
    client.publish(mqttPowerStateTopic, "OFF");
  }

//  Publish mode
  if (mode == 0) {
    client.publish(mqttModeStateTopic, "AUTO");
  } else if (mode == 1) {
    client.publish(mqttModeStateTopic, "COOL");
  } else if (mode == 2) {
    client.publish(mqttModeStateTopic, "FAN");
  } else if (mode == 3) {
    client.publish(mqttModeStateTopic, "HEAT");
  }

//  Publish temperature
  if (temperature == 0) {
    client.publish(mqttTempStateTopic, "AUTO");
  } else {
    client.publish(mqttTempStateTopic, String(temperature).c_str());
  }

//  Publish speed
  if (fanSpeed == 0) {
    client.publish(mqttSpeedStateTopic, "AUTO");
  } else if (fanSpeed == 1) {
    client.publish(mqttSpeedStateTopic, "LOW");
  } else if (fanSpeed == 2) {
    client.publish(mqttSpeedStateTopic, "MED");
  } else if (fanSpeed == 3) {
    client.publish(mqttSpeedStateTopic, "HIGH");
  } else if (fanSpeed == 4) {
    client.publish(mqttSpeedStateTopic, "SPECIAL");
  } else if (fanSpeed == 5) {
    client.publish(mqttSpeedStateTopic, "OFF");
  }
}
