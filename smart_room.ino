
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =====================================
// 1. PIN CONFIGURATION
// =====================================
#define DHT_PIN 15
#define LDR_PIN 34
#define PIR_PIN 27
#define LED_PIN 26
#define BUZZER_PIN 25

// =====================================
// 2. WI-FI CONFIGURATION
// =====================================
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// =====================================
// 3. MQTT CONFIGURATION
// =====================================
const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

const char* MQTT_TOPIC =
  "ktech/smartroom/demo2019/sensors";

const char* MQTT_COMMAND_TOPIC =
  "ktech/smartroom/demo2019/commands";

// =====================================
// 4. OBJECT CREATION
// =====================================
WiFiClient espClient;
PubSubClient mqtt(espClient);

DHTesp dht;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================
// 5. SENSOR SETTINGS
// =====================================
const int LIGHT_THRESHOLD = 1979;

// =====================================
// 6. DEVICE CONTROL SETTINGS
// =====================================

// LED manual control
bool ledManualMode = false;
bool ledManualState = false;

// Buzzer manual control
bool buzzerManualMode = false;
bool buzzerManualState = false;

// Actual device states
bool ledState = false;
bool buzzerState = false;

// =====================================
// 7. MQTT COMMAND CALLBACK
// =====================================
void mqttCallback(char* topic, byte* payload,
                  unsigned int length) {

  char message[128];

  if (length >= sizeof(message)) {
    Serial.println("Command too long!");
    return;
  }

  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.print("MQTT Command Received: ");
  Serial.println(message);

  // LED CONTROL
  if (strstr(message, "\"device\":\"led\"") != NULL) {

    if (strstr(message, "\"state\":\"ON\"") != NULL) {
      ledManualMode = true;
      ledManualState = true;
      Serial.println("LED Manual ON");
    }

    else if (strstr(message, "\"state\":\"OFF\"") != NULL) {
      ledManualMode = true;
      ledManualState = false;
      Serial.println("LED Manual OFF");
    }

    else if (strstr(message, "\"state\":\"AUTO\"") != NULL) {
      ledManualMode = false;
      Serial.println("LED Automatic Mode");
    }
  }

  // BUZZER CONTROL
  else if (strstr(message, "\"device\":\"buzzer\"") != NULL) {

    if (strstr(message, "\"state\":\"ON\"") != NULL) {
      buzzerManualMode = true;
      buzzerManualState = true;
      Serial.println("Buzzer Manual ON");
    }

    else if (strstr(message, "\"state\":\"OFF\"") != NULL) {
      buzzerManualMode = true;
      buzzerManualState = false;
      Serial.println("Buzzer Manual OFF");
    }

    else if (strstr(message, "\"state\":\"AUTO\"") != NULL) {
      buzzerManualMode = false;
      Serial.println("Buzzer Automatic Mode");
    }
  }
}

// =====================================
// 8. WI-FI CONNECTION
// =====================================
bool connectWiFi() {

  Serial.println();
  Serial.print("Connecting to Wi-Fi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED &&
         attempts < 40) {

    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("Wi-Fi CONNECTED!");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    return true;
  }

  Serial.println("Wi-Fi FAILED!");

  return false;
}

// =====================================
// 9. MQTT CONNECTION
// =====================================
bool connectMQTT() {

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  int attempts = 0;

  while (!mqtt.connected() && attempts < 5) {

    Serial.print("Connecting to MQTT...");

    String clientId =
      "KTech-ESP32-" +
      String(random(0xffff), HEX);

    if (mqtt.connect(clientId.c_str())) {

      Serial.println("MQTT CONNECTED!");

      // Subscribe to dashboard commands
      if (mqtt.subscribe(MQTT_COMMAND_TOPIC)) {
        Serial.println("Subscribed to command topic!");
      } else {
        Serial.println("Command subscription failed!");
      }

      return true;
    }

    Serial.print("MQTT FAILED. State: ");
    Serial.println(mqtt.state());

    attempts++;
    delay(2000);
  }

  Serial.println("MQTT connection unavailable.");

  return false;
}

// =====================================
// 10. SETUP
// =====================================
void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println("SMART ROOM MONITORING SYSTEM");
  Serial.println("Starting system...");

  // Connect Wi-Fi
  if (!connectWiFi()) {
    Serial.println("Wi-Fi unavailable. Will retry.");
  }

  // Initialize DHT22
  dht.setup(DHT_PIN, DHTesp::DHT22);

  // Initialize LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Welcome message
  lcd.setCursor(0, 0);
  lcd.print("SMART ROOM");

  lcd.setCursor(0, 1);
  lcd.print("MONITORING SYS");

  delay(2000);
  lcd.clear();

  // Connect MQTT
  if (WiFi.status() == WL_CONNECTED) {
    connectMQTT();
  }
}

// =====================================
// 11. MAIN LOOP
// =====================================
void loop() {

  // Check Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {

    if (!connectWiFi()) {
      delay(2000);
      return;
    }
  }

  // Check MQTT
  if (!mqtt.connected()) {

    if (!connectMQTT()) {
      delay(2000);
      return;
    }
  }

  // Receive dashboard commands
  mqtt.loop();

  // ===================================
  // 12. READ SENSORS
  // ===================================
  TempAndHumidity data =
    dht.getTempAndHumidity();

  int lightValue = analogRead(LDR_PIN);
  int motion = digitalRead(PIR_PIN);

  bool validDHT =
    !isnan(data.temperature) &&
    !isnan(data.humidity);

  // ===================================
  // 13. LIGHT DETECTION
  // ===================================
  bool isDark = lightValue < LIGHT_THRESHOLD;

  // ===================================
  // 14. LED CONTROL
  // ===================================
  if (ledManualMode) {
    ledState = ledManualState;
  } else {
    ledState = isDark;
  }

  digitalWrite(
    LED_PIN,
    ledState ? HIGH : LOW
  );

  // ===================================
  // 15. BUZZER CONTROL
  // ===================================
  if (buzzerManualMode) {
    buzzerState = buzzerManualState;
  } else {
    buzzerState = (motion == HIGH);
  }

  if (buzzerState) {
    tone(BUZZER_PIN, 2000);
  } else {
    noTone(BUZZER_PIN);
  }

  // ===================================
  // 16. LCD DISPLAY
  // ===================================
  if (validDHT) {

    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(data.temperature, 1);
    lcd.print("C H:");
    lcd.print(data.humidity, 0);
    lcd.print("%  ");

  } else {

    lcd.setCursor(0, 0);
    lcd.print("Sensor Error   ");
  }

  lcd.setCursor(0, 1);

  if (motion == HIGH) {
    lcd.print("Motion Detected ");
  }
  else if (isDark) {
    lcd.print("Room is Dark    ");
  }
  else {
    lcd.print("Room is Bright  ");
  }

  // ===================================
  // 17. PUBLISH MQTT DATA
  // ===================================
  if (validDHT) {

    char payload[200];

    snprintf(
      payload,
      sizeof(payload),
      "{\"temperature\":%.1f,\"humidity\":%.0f,"
      "\"lightValue\":%d,\"motion\":%s,"
      "\"led\":%s,\"buzzer\":%s}",
      data.temperature,
      data.humidity,
      lightValue,
      motion == HIGH ? "true" : "false",
      ledState ? "true" : "false",
      buzzerState ? "true" : "false"
    );

    if (mqtt.publish(MQTT_TOPIC, payload)) {

      Serial.print("MQTT Published: ");
      Serial.println(payload);

    } else {

      Serial.println("MQTT Publish Failed!");
    }

  } else {

    Serial.println("DHT22 reading unavailable.");
  }

  // ===================================
  // 18. SERIAL MONITOR
  // ===================================
  Serial.println("----------------------");

  Serial.print("Temperature: ");
  Serial.println(data.temperature);

  Serial.print("Humidity: ");
  Serial.println(data.humidity);

  Serial.print("Light Value: ");
  Serial.println(lightValue);

  Serial.print("Motion: ");
  Serial.println(
    motion ? "Detected" : "Not Detected"
  );

  Serial.print("LED: ");
  Serial.println(ledState ? "ON" : "OFF");

  Serial.print("Buzzer: ");
  Serial.println(buzzerState ? "ON" : "OFF");

  Serial.println(isDark ? "Room: Dark" : "Room: Bright");

  delay(1000);
}
