#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Connect to the WiFi
const char* ssid = "<wifi_name>";
const char* password = "<wifi_password>";
const char* mqtt_server = "<mqtt_hostname or ip>";

//MQTT credetials
const char* username = "arduino";
const char* password_mqtt = "4rdu1n0";

String clientName;
String mainTopic;
String subscribeTopic;
String publishTopic;
String publishMessage;

// get/arduino/#
// set/arduino/esp8266-5c:cf:7f:3d:60:97/x/x -m "xxxx"

WiFiClient espClient;
PubSubClient client(espClient);

enum sharedTopicIndex {
  TOPIC_INDEX_GET_SET = 0,
  TOPIC_INDEX_MAIN = 1,
  TOPIC_INDEX_DEVICE_ID = 2,
};

enum actionTopicIndex {
  TOPIC_INDEX_CHANNEL = 3,
  TOPIC_INDEX_ACTION = 4,
};

enum action {
  ACTION_ON = 0,
  ACTION_OFF = 1,
  ACTION_TIMMER_ON = 2,
  ACTION_TIMMER_OFF = 3,
  ACTION_PWM = 4,  
};

const byte analogInputPins[] = {A0};
const byte digitalInputPins[] = {D6, D7, D8};
const byte analogOutputPins[] = {D4, D5};
const byte digitalOutputPins[] = {D0, D1, D2, D3};

unsigned long pinsPreviousMillis[sizeof(digitalOutputPins)];
unsigned long pinsIntervals[sizeof(digitalOutputPins)];
bool pinsFinalAction[sizeof(digitalOutputPins)];

unsigned long publishInterval = 10000;
unsigned long publishPreviousMillis = 0;

enum sharedTopicIndex enumSharedTopicIndex;
enum actionTopicIndex enumActionTopicIndex;
enum action enumAccion;

void callback(char* topic, byte* payload, unsigned int length) {

  String topicsChain = String (topic);
  byte numberOfTopics = getNumberOfTopics(topicsChain);
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Received -> ");
  Serial.print("Topic: ");
  Serial.print(topicsChain);
  Serial.print("Message: ");
  Serial.println(message);

  int i = 0;
  char* p = strtok (topic, "/");
  char* topics[numberOfTopics];

  while (p != NULL)
  {
    topics[i++] = p;
    p = strtok (NULL, "/");
  }

  unsigned int action = atoi(topics[TOPIC_INDEX_ACTION]);
  unsigned int channel = atoi(topics[TOPIC_INDEX_CHANNEL]);

  if (action == ACTION_ON) {
    encender(channel);
  } else if (action == ACTION_OFF) {
    apagar(channel);
  } else if (action == ACTION_TIMMER_ON) {
    pinsIntervals[channel] = message.toInt();
    pinsPreviousMillis[channel] = millis();
  } else {
    Serial.println("ERROR: Action does not exists");
  }
}

void reconnect() {

  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect

    if (client.connect((const char*)clientName.c_str(), username, password_mqtt)) {
      Serial.println("Connected");
      client.subscribe((const char*)subscribeTopic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{

  Serial.begin(9600);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);


  String clientNameStr;
  clientNameStr += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientNameStr += macToStr(mac);
  //    clientName += "-";
  //    clientName += String(micros() & 0xff, 16);

  clientName = clientNameStr;

  Serial.print("Client name: ");
  Serial.print(clientName);
  Serial.print(" - username: ");
  Serial.print(username);
  Serial.print(" - password: ");
  Serial.println(password_mqtt);

  String mainTopicStr;
  mainTopicStr += "arduino/";
  mainTopicStr += clientNameStr;

  mainTopic = mainTopicStr;

  Serial.print("Main Topic: ");
  Serial.println(mainTopic);

  String subscribeTopicStr;
  subscribeTopicStr += "set/";
  subscribeTopicStr += mainTopicStr;
  subscribeTopicStr += "/#";

  subscribeTopic = subscribeTopicStr;

  for (int i = 0; i < sizeof(digitalOutputPins); i++) {
    pinMode(digitalOutputPins[i], OUTPUT);
  }

  for (int i = 0; i < sizeof(digitalInputPins); i++) {
    pinMode(digitalInputPins[i], INPUT);
  }
}

void loop()
{

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  checkPulsos();

  publishPins();
}

void encender(byte channel) {

  digitalWrite(digitalOutputPins[channel], LOW);
}

void apagar(byte channel) {

  digitalWrite(digitalOutputPins[channel], HIGH);
}

void checkPulsos() {

  for (int i = 0; i < sizeof(digitalOutputPins); i++) {
    if (pinsIntervals[i] > 0 && millis() - pinsPreviousMillis[i] < pinsIntervals[i]) {
      encender(i);
      pinsFinalAction[i] = true;
    } else {
      if (pinsFinalAction[i]) {
        apagar(i);
        pinsFinalAction[i] = false;
        pinsIntervals[i] = 0;
      }
    }
  }
}

void publishPins() {

  if (millis() - publishPreviousMillis >= publishInterval) {

    for (int i = 0; i < sizeof(digitalInputPins); i++) {

      String topic;
      topic += "get/";
      topic += mainTopic;
      topic += "/digital/";
      topic += i; //channel

      publishTopic = topic;

      String message;
      message += digitalRead(digitalInputPins[i]);

      publishMessage = message;

      publishState((char*)publishTopic.c_str(), (char*) publishMessage.c_str());
    }

    for (int i = 0; i < sizeof(analogInputPins); i++) {
      //publish
      String topic;
      topic += "get/";
      topic += mainTopic;
      topic += "/analog/";
      topic += i; //channel

      publishTopic = topic;

      String message;
      message += analogRead(analogInputPins[i]);

      publishMessage = message;

      publishState((char*)publishTopic.c_str(), (char*) publishMessage.c_str());
    }

    publishPreviousMillis = millis();
  }
}

int publishState(char* topic, char* message) {

  Serial.print("Published -> ");
  Serial.print("Topic: ");
  Serial.print(topic);
  Serial.print(" - Message: ");
  Serial.println(message);

  return client.publish(topic, message);
}

byte getNumberOfTopics(String topicChain) {

  byte numberOfTopics = 1;
  for (int i = 0; i < topicChain.length(); i++) {
    if (topicChain.charAt(i) == '/') {
      numberOfTopics++;
    }
  }
  return numberOfTopics;
}

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}




