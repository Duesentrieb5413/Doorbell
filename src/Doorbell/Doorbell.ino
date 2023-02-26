#include "Config.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <NTPClient.h>

WiFiClient espClient;
PubSubClient MqttClient(espClient);
const long utcOffsetInSeconds = 3600;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

bool bellActive = true;
bool bellRing = false;

bool bellButtonPrevious;
bool bellButtonCurrent;

int deactivationDuration;
int activationMinute = -1;
int activationHour = -1;

unsigned long ringStartTime;
unsigned long timerTime = 0;

void setup() {
  Serial.begin(115200);
  //Reset relay
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, HIGH);
  pinMode(BELLBUTTON, INPUT_PULLUP);

  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.hostname(hostname);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setPort(8266);

  ArduinoOTA.setPassword(otaPassword);

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
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  timeClient.begin();
}

void loop() {
  ArduinoOTA.handle();

  connectMqtt();
  MqttClient.loop();

  // Evaluate bell button
  bellButtonCurrent = digitalRead(BELLBUTTON);
  if (!bellButtonCurrent and bellButtonPrevious) {
    MqttClient.publish("Doorbell/BellButton", "1");
    // Set bell to ring
    Serial.println("Set bell to ring from button");
    bellRing = true;
    ringStartTime = millis();
  } else if (bellButtonCurrent and !bellButtonPrevious) {
    MqttClient.publish("Doorbell/BellButton", "0");
  }
  // Save status for edge detection
  bellButtonPrevious = bellButtonCurrent;

  // Evaluate bell ringing
  if (bellActive & bellRing) {
    Serial.println("bellActive & bellRing");
    // Activate bell ring for the predefined time
    if (millis() - ringStartTime <= ringTime) {
      Serial.println("Bell rings - relay triggered");
      digitalWrite(RELAY, LOW);
      MqttClient.publish("Doorbell/BellRing", "1");
    } else {
      Serial.println("Bell ring stop - relay triggered");
      digitalWrite(RELAY, HIGH);
      MqttClient.publish("Doorbell/BellRing", "0");
      bellRing = false;
    }
  }

  // Tasks executed every second
  if (millis() - timerTime >= 1000) {
    timerTime += 1000;

    // Activate bell at midnight if deactivated
    timeClient.update();
    if (timeClient.getHours() == 0 && timeClient.getMinutes() == 0 && timeClient.getSeconds() == 1) {
      if (bellActive == 0) {
        bellActive = 1;
        MqttClient.publish("Doorbell/BellActive", "1");
        MqttClient.publish("Doorbell/Status", "Bell automatically activated at midnight");
        Serial.println("Bell automatically activated at midnight");
      }
    }

    // Check automatic activation
    if (activationHour >= 0 && activationMinute >= 0) {
      if (timeClient.getHours() == activationHour && timeClient.getMinutes() == activationMinute) {
        bellActive = 1;
        MqttClient.publish("Doorbell/BellActive", "1");
        String mqttMessage = F("Bell activated at ");
        mqttMessage.concat(timeClient.getFormattedTime());
        MqttClient.publish("Doorbell/Status", mqttMessage.c_str());
        Serial.println(mqttMessage);

        activationHour = -1;
        activationMinute = -1;
      }
    }

    // Set activation time
    if (deactivationDuration > 0) {
      activationMinute = timeClient.getMinutes() + deactivationDuration;
      int additionalHours = 0;
      if (activationMinute >= 60) {
        additionalHours = activationMinute / 60;
        activationMinute = activationMinute % 60;
      }

      activationHour = timeClient.getHours() + additionalHours;
      if (activationHour >= 24) {
        activationHour = activationHour % 24;
      }

      String mqttMessage = F("Bell will be activated at ");
      if (activationHour < 10)
        mqttMessage.concat(F("0"));
      mqttMessage.concat(activationHour);
      mqttMessage.concat(F(":"));
      if (activationMinute < 10)
        mqttMessage.concat(F("0"));
      mqttMessage.concat(activationMinute);
      MqttClient.publish("Doorbell/Status", mqttMessage.c_str());
      Serial.println(mqttMessage);

      deactivationDuration = 0;
    }
  }
}

boolean connectMqtt() {
  char* MqttUser = NULL;
#ifdef MqttUsername
  MqttUser = MqttUsername;
#endif
  const char* MqttPass = NULL;
#ifdef MqttPassword
  MqttPass = MqttPassword;
#endif
  if (!MqttClient.connected()) {
    MqttClient.setServer(MqttBroker, 1883);
    int retries = 0;
    while (!MqttClient.connected() && retries < 3) {
      MqttClient.connect("Doorbell", MqttUser, MqttPass);
      retries++;
      if (!MqttClient.connected()) {
        delay(1000);
        Serial.println(F("Failed to connect to Mqtt broker, retrying..."));
      } else {
        Serial.println(F("MqttClient connected"));
        retries = 0;
#ifdef MqttSubscriptionPrefix
        char topic[30];
        sprintf(topic, "%s/%s/#", MqttSubscriptionPrefix, MqttTopicPrefix);
        while (!MqttClient.subscribe(topic) && retries < 3) {
          retries++;
          Serial.println(F("Failed to subscribe to Mqtt topic ("));
          Serial.print(topic);
          Serial.print(F("), retrying..."));
        }
        if (retries < 3) {
          Serial.print(F("Subscribed to MQTT topic: "));
          Serial.println(topic);
        }
        MqttClient.setKeepAlive(120);
        MqttClient.setCallback(subscribeReceive);
#endif
        return true;
      }
    }
    return false;
  } else {
    return true;
  }
  return false;
}

// Recieve Mqtt messages from the subscribed topic and evaluate
void subscribeReceive(char* topic, byte* payload, unsigned int length) {
#ifdef MqttSetTopicRing
  if (String(topic) == MqttSetTopicRing) {
    String payloadString((char*)payload);
    payloadString.remove(length);

    Serial.print("Received message from MQTT subscription with topic '");
    Serial.print(topic);
    Serial.print("' and payload '");
    Serial.print(payloadString);
    Serial.println("'");

    // Deactivate bell
    if (strncmp((char*)payload, "Deactivate", length) == 0) {
      bellActive = false;
      MqttClient.publish("Doorbell/BellActive", "0");
      bellRing = false;
      MqttClient.publish("Doorbell/BellRing", "0");
      Serial.println("External set ring activation to 0");
    }

    // Deactivate bell for specific time
    else if (strncmp((char*)payload, "Deactivate", 10) == 0) {
      String payloadString = (char*)payload;
      //int duration = atoi(payloadString.substring(10, length));
      deactivationDuration = payloadString.substring(10, length).toInt();
      String mqttMessage = F("Deactivate for ");
      mqttMessage.concat(deactivationDuration);
      mqttMessage.concat(F(" minutes"));
      MqttClient.publish("Doorbell/Status", mqttMessage.c_str());
      bellActive = false;
      MqttClient.publish("Doorbell/BellActive", "0");
      bellRing = false;
      MqttClient.publish("Doorbell/BellRing", "0");
      Serial.println("External set ring activation to 0");
    }

    // Activate bell
    else if (strncmp((char*)payload, "Activate", length) == 0) {
      bellActive = true;
      MqttClient.publish("Doorbell/BellActive", "1");
      Serial.println("External set ring activation to 1");

      activationHour = -1;
      activationMinute = -1;
    }

    // Ring bell once
    else if (strncmp((char*)payload, "Once", length) == 0) {
      if (bellActive) {
        bellRing = true;
        ringStartTime = millis();
        MqttClient.publish("Doorbell/Status", "Ring triggered");
        Serial.println("External ringing triggered");
      } else {
        MqttClient.publish("Doorbell/Status", "Bell not active");
        Serial.println("External ringing triggered, but bell not active");
      }
    }

  } else {
    Serial.println("Topic received - no command found");
    // Print the topic
    Serial.print("Topic: ");
    Serial.println(topic);

    // Print the message
    Serial.print("Message: ");
    for (unsigned int i = 0; i < length; i++) {
      Serial.print(char(payload[i]));
    }
    Serial.println();
    Serial.print("Length: ");
    Serial.println(length);
  }
#endif
}