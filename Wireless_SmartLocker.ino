#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9
#define LED_G 5 //define green LED pin
#define LED_R 4 //define red LED
#define BUZZER 2 //buzzer pin
#define ACCESS_DELAY 2000
#define DENIED_DELAY 1000
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.

int alarm = 0;
uint8_t alarmStat = 0;
uint8_t maxError = 5;
int relay_pin = 3;
int door_status = 0;

String rfid_tag;

WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "ssid";
const char* password = "pass"; 
const char* mqtt_server = "ip_Address";

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected - ESP IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(String topic, byte* message, unsigned int length) {
  String messageTemp;
  String JSONMessage;
  Serial.print("Message arrived on topic: "); Serial.println(topic);
  Serial.print("Message: ");
  
  //MQTT SUBSCRIBER-----------------------------------------//
  if(topic == "locker/door_status") {
    for (int i = 0; i < length; i++) {
      Serial.print((char)message[i]);
      JSONMessage += (char)message[i];
    }
    Serial.print("Initial string value: "); Serial.println(JSONMessage);
    
    StaticJsonBuffer<300> JSONBuffer;   //Memory pool
    JsonObject& parsed = JSONBuffer.parseObject(JSONMessage); //Parse message
   
    if (!parsed.success()) {   //Check for errors in parsing
      Serial.println("Parsing failed");
      return;
    }

    const char * dataType = parsed["dataType"]; //Get data type value
    Serial.print("Data type: ");  Serial.println(dataType);

    door_status = parsed["values"][0];
  }
  //-----------------------------------------------------------------------//  
}

void reconnect() {
  
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266")) {
      Serial.println("connected");

      //PINGPONG SUBSCRIPTION
      client.subscribe("locker/door_status");
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  SPI.begin();          // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522
  pinMode(relay_pin, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  noTone(BUZZER);
  Serial.println("Put your card to the reader...");
  Serial.println();
}

void loop() {
  if (!client.connected())  {reconnect();}
  if (!client.loop())       {client.connect("ESP8266");}

  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {return;}
  
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {return;}
  
  //Show UID on serial monitor
  Serial.print("UID tag :");
  String content= "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
     Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
     Serial.print(mfrc522.uid.uidByte[i], HEX);
     content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  
  Serial.println();
  Serial.print("Message : ");
  content.toUpperCase();

  rfid_tag = content;

  //----MQTT PUBLISHER------------------------------------//
  StaticJsonBuffer<300> JSONbuffer;
  JsonObject& JSONencoder = JSONbuffer.createObject();

  JSONencoder["dataType"] = "RFID_Tag";
  JsonArray& values = JSONencoder.createNestedArray("values");

  values.add(rfid_tag);

  char JSONmessageBuffer[100];
  JSONencoder.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
  Serial.println("Sending message to MQTT topic..");
  Serial.println(JSONmessageBuffer);
      
  if (client.publish("locker/RFID_Tag", JSONmessageBuffer) == true) {
    Serial.println("Success sending message");
  } else {
    Serial.println("Error sending message");
  }
  client.loop();
  Serial.println("-------------");
  //----------------------------------------------------------------------------------------------------------//
  
  if (door_status == 1) { //change here the UID of the card/cards that you want to give access
    Serial.println("AKSES DITERIMA");
    Serial.println();
    delay(250);
    digitalWrite(LED_G, HIGH);
    digitalWrite(BUZZER, HIGH);
    digitalWrite(relay_pin, LOW);
    delay(170);
    digitalWrite(BUZZER, LOW);
    delay(250);
    digitalWrite(BUZZER, HIGH);
    delay(250);
    digitalWrite(BUZZER, LOW);
    delay(200);
    delay(ACCESS_DELAY);
    digitalWrite(LED_G, LOW); 
     
    digitalWrite(relay_pin, HIGH);
    door_status = 0;
    delay(150);
  }
 
 else   {
    Serial.println("AKSES DITOLAK");
    digitalWrite(LED_R, HIGH);
    digitalWrite(BUZZER, HIGH);
    delay(800);
    digitalWrite(BUZZER, LOW);
    delay(DENIED_DELAY);
    digitalWrite(LED_R, LOW);
    noTone(BUZZER);
  }
}
