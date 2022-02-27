// #==================================================================#
// ‖ Author: Luis Alejandro Domínguez Bueno (LADBSoft)                ‖
// ‖ Date: 2021-09-25                                    Version: 1.0 ‖
// #==================================================================#
// ‖ Name: ESP8266 Generic 3-speed fan MQTT controller                ‖
// ‖ Description: A sketch for the NodeMCU ESP8266 microcontroller    ‖
// ‖ board that adds IoT functionality to a generic 3-speed foot or   ‖
// ‖ pedestal fan. The program is design to work with a fan which     ‖
// ‖ motor is controlled by choosing ONE of 3 coil cables, and NOT    ‖
// ‖ for those which speeds are chosen by powering one, two or the    ‖
// ‖ three coils at the same time (though that change should be easy. ‖
// ‖ You should try 😉).                                              ‖
// ‖                                                                  ‖
// ‖ The sketch uses 3 pins as inputs, for three toggle buttons (one  ‖
// ‖ for each speed) and 3 pins as outputs, for each of the three     ‖
// ‖ speeds.                                                          ‖
// ‖                                                                  ‖
// ‖ The speed can be controlled both manually with input buttons and ‖
// ‖ via Wi-Fi. The last command received will have priority over the ‖
// ‖ other. Example: If the "2" speed button is pushed and a "3"      ‖
// ‖ command is received via Wi-Fi, it will be listened to, and the   ‖
// ‖ speed will change to "3". If then the "1" button is pushed, the  ‖
// ‖ speed will change to "1".                                        ‖
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
#include "configuration.h"

// +------------------------------------------------------------------+
// |                         G L O B A L S                            |
// +------------------------------------------------------------------+

WiFiClient espClient;
PubSubClient client(espClient);
long lastCheck = 0;
byte currentSpeed = 0;
long lastButtonStateTime = 0;
byte currentButtonState = 0;  // Buttons states in binary, using the 3 LSB

// +------------------------------------------------------------------+
// |                     S U B R O U T I N E S                        |
// +------------------------------------------------------------------+

void setup_wifi() {
  WiFiManager wifiManager;
  wifiManager.setTimeout(180); //3 minutes

  if(!wifiManager.autoConnect(wifiSsid, wifiPassword)) {
    Serial.print("Couldn't connect to Wi-Fi (");
    Serial.print(wifiSsid);
    Serial.println("). Resetting...");
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
  Serial.print("Received message from topic ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(payloadString);

// Conversion of payload to Byte
  payloadByte = (byte)payloadString.toInt();

// Speed Topic: Payload will be "0", "1", "2", or "3"
  if(topicString.equals(String(mqttSpeedCommandTopic))) {
    if (payloadByte == 0) {
      updateRelayState(0b00000000);  // All OFF
    } else if (payloadByte == 1) {
      updateRelayState(0b00000001);  // Speed 1
    } else if (payloadByte == 2) {
      updateRelayState(0b00000010);  // Speed 2
    } else if (payloadByte == 3) {
      updateRelayState(0b00000100);  // Speed 3
    }
  }

  publishStates();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqttClientId, mqttUser, mqttPassword)) {
      Serial.println("connected");
      // Once connected, resubscribe
      client.subscribe(mqttSpeedCommandTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void publishStates() {
//  Publish speed
  if (currentSpeed == 0) {
    client.publish(mqttSpeedStateTopic, "0");
  } else if (currentSpeed == 1) {
    client.publish(mqttSpeedStateTopic, "1");
  } else if (currentSpeed == 2) {
    client.publish(mqttSpeedStateTopic, "2");
  } else if (currentSpeed == 3) {
    client.publish(mqttSpeedStateTopic, "3");
  }
}

byte getButtonState() {
  byte stateReturn = 0b00000000;
  
  byte buttonState = digitalRead(speed1ButtonPin);
  if (buttonState == LOW) {
    stateReturn = stateReturn | 0b00000001;
  }

  buttonState = digitalRead(speed2ButtonPin);
  if (buttonState == LOW) {
    stateReturn = stateReturn | 0b00000010;
  }
  
  buttonState = digitalRead(speed3ButtonPin);
  if (buttonState == LOW) {
    stateReturn = stateReturn | 0b00000100;
  }

  lastButtonStateTime = millis();

  return stateReturn;
}

void updateRelayState(byte relayState) {
  if (relayState & 0b00000100) {  // Speed 3
    digitalWrite(speed1RelayPin, LOW);
    digitalWrite(speed2RelayPin, LOW);
    if (currentSpeed > 0) {
      delay(100);
    }
    digitalWrite(speed3RelayPin, HIGH);
    currentSpeed = 3;
  } else if (relayState & 0b00000010) {  // Speed 2
    digitalWrite(speed1RelayPin, LOW);
    digitalWrite(speed3RelayPin, LOW);
    if (currentSpeed > 0) {
      delay(100);
    }
    digitalWrite(speed2RelayPin, HIGH);
    currentSpeed = 2;
  } else if (relayState & 0b00000001) {  // Speed 1
    digitalWrite(speed2RelayPin, LOW);
    digitalWrite(speed3RelayPin, LOW);
    if (currentSpeed > 0) {
      delay(100);
    }
    digitalWrite(speed1RelayPin, HIGH);
    currentSpeed = 1;
  } else {  // Speed 0
    digitalWrite(speed1RelayPin, LOW);
    digitalWrite(speed2RelayPin, LOW);
    digitalWrite(speed3RelayPin, LOW);
    currentSpeed = 0;
  }
}

// +------------------------------------------------------------------+
// |                           S E T U P                              |
// +------------------------------------------------------------------+

void setup() {
  Serial.begin(115200);
  
  pinMode(speed1ButtonPin, INPUT_PULLUP);
  pinMode(speed2ButtonPin, INPUT_PULLUP);
  pinMode(speed3ButtonPin, INPUT_PULLUP);
  pinMode(speed1RelayPin, OUTPUT);
  pinMode(speed2RelayPin, OUTPUT);
  pinMode(speed3RelayPin, OUTPUT);

  setup_wifi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

// +------------------------------------------------------------------+
// |                            L O O P                               |
// +------------------------------------------------------------------+

void loop() {
  String payload;
  long now = millis();
  byte buttonState = 0;
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (now - lastButtonStateTime > 250) {
    buttonState = getButtonState();

    if (buttonState != currentButtonState) {
      Serial.print("Button state changed: ");
      Serial.println(buttonState, BIN);
    
      updateRelayState(buttonState);
      publishStates();

      currentButtonState = buttonState;
    }
  }

  if (now - lastCheck > 5000) {
    lastCheck = now;

    publishStates();
  }
}
