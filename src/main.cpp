#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// NTP Settings
#define NTP_SERVER     "th.pool.ntp.org"
#define UTC_OFFSET     7*3600 
#define UTC_OFFSET_DST 0

#define DURATION_STRING_LENGTH 9 
#define TIMESTAMP_STRING_LENGTH 20
char duration[DURATION_STRING_LENGTH]; // เก็บค่าเวลา
char timestamp[TIMESTAMP_STRING_LENGTH]; // เก็บค่าวันเดือนปีและเวลา

// WiFi Settings
const char* ssid = "******";
const char* password = "*******";

// MQTT Settings
const char* mqtt_server   = "188.166.191.227";
const int mqtt_port       = 1883;
const char* mqtt_username = "HW_MQTT_01";
const char* mqtt_password = "hw_mqtt_beonit";
const char* mqtt_topic    = "HW_mqtt/testing/01";

WiFiClient espClient;
PubSubClient client(espClient);

// LED/Swicth Settings
const int buttonPins[] = {25}; // ขาที่ใช้เชื่อมกับปุ่ม
const int ledPins[] = {12}; // ขาที่ใช้เชื่อมกับไฟ LED
const int numButtons = 1; // จำนวนปุ่ม
bool isButtonPressed[numButtons] = {false}; // เก็บสถานะปุ่ม
bool isLightOn[numButtons] = {false};       // เก็บสถานะไฟ

unsigned long debounceDelay = 200; // เวลาที่รอสำหรับการดับเบิ้ล (milliseconds)
unsigned long lastDebounceTime[numButtons] = {0}; // เวลาล่าสุดที่มีการเปลี่ยนแปลงของสถานะปุ่ม

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;
  }
  strftime(duration, DURATION_STRING_LENGTH, "%H:%M:%S", &timeinfo);
  strftime(timestamp, TIMESTAMP_STRING_LENGTH, "%d/%m/%Y,%H:%M:%S", &timeinfo);
}

void setup_wifi() {
  delay(10);
  // Connect to WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected - IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Handle incoming messages
}

void call() {
  for (int i = 0; i < numButtons; ++i) {
    // Create JSON object
    StaticJsonDocument<200> jsonDoc;
    jsonDoc["Status"] = isLightOn[i] ? "ON" : "OFF";
    jsonDoc["Duration"] = duration;
    jsonDoc["Esp32Time"] = timestamp;
    jsonDoc["LocalIP"] = WiFi.localIP();

    // Serialize JSON to a char array
    char buffer[256];
    serializeJson(jsonDoc, buffer);

    // Publish JSON to MQTT topic
    client.publish(mqtt_topic, buffer);
    Serial.println(buffer);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup_MQTT() {
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void setup_LED_Switch() {
  for (int i = 0; i < numButtons; ++i) {
    pinMode(ledPins[i], OUTPUT);      // ตั้งโหมดขา LED เป็น OUTPUT
    pinMode(buttonPins[i], INPUT);    // ตั้งโหมดขาปุ่มเป็น INPUT
    digitalWrite(ledPins[i], LOW);    // ปิดไฟ LED เริ่มต้น
  }
}

void setup_NTP() {
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
}

bool buttonPressed(int buttonIndex) {
  // ตรวจสอบสถานะปุ่ม
  bool currentState = digitalRead(buttonPins[buttonIndex]);
  // ตรวจสอบการเปลี่ยนแปลงของสถานะปุ่มโดยใช้การตรวจสอบเวลา
  if (currentState != isButtonPressed[buttonIndex] && millis() - lastDebounceTime[buttonIndex] > debounceDelay) {
    isButtonPressed[buttonIndex] = currentState; // อัปเดตสถานะปุ่ม
    return isButtonPressed[buttonIndex]; // ส่งค่าสถานะปุ่มให้กลับไป
  }
  return false; // ถ้ายังไม่มีการกดปุ่ม ส่งค่า false
}

void setup() {
  Serial.begin(115200);
  setup_LED_Switch();
  setup_wifi();
  setup_NTP();
  setup_MQTT();

}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  for (int i = 0; i < numButtons; ++i) {
    if (buttonPressed(i)) {
      // ตรวจสอบการกดดับเบิ้ล
      if (millis() - lastDebounceTime[i] < 1000) {
        client.loop();
        // ถ้าเป็นการกดดับเบิ้ล
        isLightOn[i] = !isLightOn[i];
        // เปิดหรือปิดไฟตามสถานะปัจจุบัน
        digitalWrite(ledPins[i], isLightOn[i] ? HIGH : LOW);
        printLocalTime();//เรียกใช้เวลา printLocalTime เพื่อเก็บ NTP
        call();//เรียกใช้ call เพื่อส่งข้อมูลส่งขึ้น Server
        //Serial.print("LED ");
        //Serial.print(i + 1);
        //Serial.println(isLightOn[i] ? " ON" : " OFF");
      }
      // อัปเดตเวลาล่าสุดที่มีการเปลี่ยนแปลงของสถานะปุ่ม
      lastDebounceTime[i] = millis();
    }
  }
//delay(1000); // Adjust delay as needed
}


