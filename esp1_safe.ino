// ====================================================================
// ESP1 - Environmental Monitoring & Control
// SAFE VERSION - No hardcoded secrets!
// All secrets are in config.h (NOT uploaded to GitHub)
// ====================================================================

#define BLYNK_PRINT Serial

// ===== Include Configuration (LOCAL ONLY - NOT ON GITHUB) =====
#include "config.h"

// ===== Blynk Setup =====
#define BLYNK_AUTH_TOKEN BLYNK_AUTH_TOKEN_ESP1

// ===== InfluxDB Setup =====
#define INFLUXDB_URL INFLUXDB_URL
#define INFLUXDB_ORG INFLUXDB_ORG
#define INFLUXDB_BUCKET INFLUXDB_BUCKET
#define INFLUXDB_TOKEN INFLUXDB_TOKEN

// ===== Libraries =====
#include <WiFiClientSecure.h>
#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <TinyGPS++.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// ===== Time Zone =====
#define TZ_INFO "EET-2EEST,M3.5.5/0,M10.5.5/1"

// ===== InfluxDB Client =====
InfluxDBClient influxClient(
    INFLUXDB_URL,
    INFLUXDB_ORG,
    INFLUXDB_BUCKET,
    INFLUXDB_TOKEN,
    InfluxDbCloud2CACert
);
Point animalData("animal_house");

// ===== WiFi Credentials (from config.h) =====
char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASSWORD;

// ===== Pin Definitions =====
#define GPS_RX 16
#define GPS_TX 17
#define GPS_BAUD 9600

#define TEMP_PIN 4
#define LDR_PIN 6
#define PIR_PIN 15
#define SERVO_PIN 18
#define SERVO_PIN2 19
#define RAIN_SENSOR_PIN 21

#define FAN1 8
#define FAN2 9
#define FAN3 10
#define BUZZER 5
#define LED1_PIN 13

// ===== GPS =====
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// ===== Servos =====
Servo roofServo;
Servo roofServo2;

// ===== Servo Angles =====
#define SERVO_OPEN_ANGLE 90
#define SERVO_CLOSED_ANGLE 0
#define SERVO2_OPEN_ANGLE 180
#define SERVO2_CLOSED_ANGLE 90

// ===== Blynk Virtual Pins =====
#define V_TEMP V10
#define V_LIGHT V11
#define V_MOTION V13
#define VPIN_RAIN V22
#define V_GPS_MAP V8
#define V_FAN1_CONTROL V1
#define V_FAN2_CONTROL V2
#define V_FAN3_CONTROL V3
#define V_LED_CONTROL V7
#define V_BUZZER_CONTROL V4
#define V_ROOF_OPEN V26
#define V_ROOF_CLOSE V27
#define V_ROOF_STATUS V25
#define TEST V28
#define V_GPS_SATS V30
#define V_GPS_COORD V31
#define V_GPS_DEBUG V32
#define V_GPS_STATUS V33

// ===== Global Variables =====
float temperatureC = 0;
int lightValue = 0;
bool motion = false;
bool rainDetected = false;
bool manualFan1 = false;
bool manualFan2 = false;
bool manualFan3 = false;
bool manualLED = false;
bool manualBuzzer = false;
bool tempAlertSent = false;
bool lastRainState = false;
bool roofIsOpen = false;

// ====================================================================
// ROOF CONTROL FUNCTIONS
// ====================================================================

void OpenRoof() {
    if (roofIsOpen) return;
    roofServo.write(SERVO_OPEN_ANGLE);
    roofServo2.write(SERVO2_OPEN_ANGLE);
    delay(500);
    roofIsOpen = true;
    Blynk.virtualWrite(V_ROOF_STATUS, "Roof Open");
}

void CloseRoof() {
    if (!roofIsOpen) return;
    roofServo.write(SERVO_CLOSED_ANGLE);
    roofServo2.write(SERVO2_CLOSED_ANGLE);
    delay(500);
    roofIsOpen = false;
    Blynk.virtualWrite(V_ROOF_STATUS, "Roof Closed");
}

void setupRoofServos() {
    if (roofServo.attached()) { roofServo.detach(); delay(100); }
    if (roofServo2.attached()) { roofServo2.detach(); delay(100); }
    
    roofServo.setPeriodHertz(50);
    roofServo2.setPeriodHertz(50);
    
    roofServo.attach(SERVO_PIN, 500, 2500);
    roofServo2.attach(SERVO_PIN2, 500, 2500);
    
    roofServo.write(SERVO_CLOSED_ANGLE);
    roofServo2.write(SERVO2_CLOSED_ANGLE);
    roofIsOpen = false;
    delay(300);
}

void checkAndControlRoof() {
    bool currentRain = (digitalRead(RAIN_SENSOR_PIN) == LOW);
    Blynk.virtualWrite(VPIN_RAIN, currentRain ? "Rain Detected" : "No Rain");
    
    if (currentRain && roofIsOpen) {
        CloseRoof();
        Blynk.logEvent("rain_alert", "Rain detected! Roof closed.");
    }
}

// ====================================================================
// BLYNK WIDGET HANDLERS
// ====================================================================

BLYNK_WRITE(V_FAN1_CONTROL) {
    int v = param.asInt();
    if (v == 2) {
        manualFan1 = false;
    } else {
        manualFan1 = true;
        digitalWrite(FAN1, v);
    }
}

BLYNK_WRITE(V_FAN2_CONTROL) {
    int v = param.asInt();
    if (v == 2) {
        manualFan2 = false;
    } else {
        manualFan2 = true;
        digitalWrite(FAN2, v);
    }
}

BLYNK_WRITE(V_FAN3_CONTROL) {
    int v = param.asInt();
    if (v == 2) {
        manualFan3 = false;
    } else {
        manualFan3 = true;
        digitalWrite(FAN3, v);
    }
}

BLYNK_WRITE(V_LED_CONTROL) {
    int v = param.asInt();
    if (v == 2) {
        manualLED = false;
    } else {
        manualLED = true;
        digitalWrite(LED1_PIN, v);
    }
}

BLYNK_WRITE(V_BUZZER_CONTROL) {
    int v = param.asInt();
    if (v == 2) {
        manualBuzzer = false;
    } else {
        manualBuzzer = true;
        digitalWrite(BUZZER, v);
    }
}

BLYNK_WRITE(V_ROOF_OPEN) {
    int value = param.asInt();
    if (value == 1) { OpenRoof(); }
}

BLYNK_WRITE(V_ROOF_CLOSE) {
    int value = param.asInt();
    if (value == 1) { CloseRoof(); }
}

// ====================================================================
// SENSOR READING FUNCTIONS
// ====================================================================

float readTemperature() {
    int raw = analogRead(TEMP_PIN);
    float voltage = raw * (3.3f / 4095.0f);
    temperatureC = voltage * 100.0f;
    
    if (temperatureC > 35 && !tempAlertSent) {
        Blynk.logEvent("temp_alert", "High temperature detected!");
        tempAlertSent = true;
    }
    if (temperatureC <= 35) { tempAlertSent = false; }
    
    if (!manualFan1 && !manualFan2 && !manualFan3) {
        if (temperatureC < 20) {
            digitalWrite(FAN1, LOW);
            digitalWrite(FAN2, LOW);
            digitalWrite(FAN3, LOW);
        } else if (temperatureC < 25) {
            digitalWrite(FAN1, HIGH);
            digitalWrite(FAN2, LOW);
            digitalWrite(FAN3, LOW);
        } else if (temperatureC < 30) {
            digitalWrite(FAN1, HIGH);
            digitalWrite(FAN2, HIGH);
            digitalWrite(FAN3, LOW);
        } else {
            digitalWrite(FAN1, HIGH);
            digitalWrite(FAN2, HIGH);
            digitalWrite(FAN3, HIGH);
        }
    }
    return temperatureC;
}

int readLightLevel() {
    lightValue = analogRead(LDR_PIN);
    if (!manualLED) {
        digitalWrite(LED1_PIN, (lightValue > 3000) ? LOW : HIGH);
    }
    return lightValue;
}

bool readMotion() {
    motion = digitalRead(PIR_PIN);
    if (!manualBuzzer) {
        digitalWrite(BUZZER, motion ? HIGH : LOW);
    }
    return motion;
}

// ====================================================================
// INFLUXDB FUNCTIONS
// ====================================================================

void sendDataToInfluxDB() {
    if (WiFi.status() != WL_CONNECTED) { return; }
    
    bool rain = (digitalRead(RAIN_SENSOR_PIN) == LOW);
    
    animalData.clearFields();
    animalData.addField("temperature", temperatureC);
    animalData.addField("light", lightValue);
    animalData.addField("motion", motion ? 1 : 0);
    animalData.addField("rain", rain ? 1 : 0);
    animalData.addField("roof_state", roofIsOpen ? 1 : 0);
    animalData.addField("fan1", digitalRead(FAN1));
    animalData.addField("fan2", digitalRead(FAN2));
    animalData.addField("fan3", digitalRead(FAN3));
    animalData.addField("buzzer", digitalRead(BUZZER));
    animalData.addField("led", digitalRead(LED1_PIN));
    
    influxClient.writePoint(animalData);
}

// ====================================================================
// GPS FUNCTIONS
// ====================================================================

unsigned long lastGpsSend = 0;
unsigned long lastGpsDebug = 0;
bool gpsTested = false;

void sendGpsToBlynk() {
    while (gpsSerial.available() > 0) {
        char c = gpsSerial.read();
        gps.encode(c);
    }
    
    unsigned long now = millis();
    
    if (now - lastGpsDebug >= 2000) {
        lastGpsDebug = now;
        int sats = gps.satellites.value();
        Blynk.virtualWrite(V_GPS_SATS, sats);
        
        if (gps.location.isValid()) {
            Blynk.virtualWrite(V_GPS_STATUS, "FIXED");
            String debugMsg = "GPS FIXED! Sats: " + String(sats);
            Blynk.virtualWrite(V_GPS_DEBUG, debugMsg);
            Blynk.virtualWrite(TEST, "GPS: Fixed");
        } else {
            Blynk.virtualWrite(V_GPS_STATUS, "NO FIX");
            String debugMsg = "No fix | Sats: " + String(sats);
            Blynk.virtualWrite(V_GPS_DEBUG, debugMsg);
            Blynk.virtualWrite(TEST, "GPS: Waiting...");
        }
    }
    
    if (now - lastGpsSend >= 5000) {
        lastGpsSend = now;
        if (gps.location.isValid()) {
            Blynk.virtualWrite(V_GPS_MAP, gps.location.lat(), gps.location.lng());
            String coords = "Lat: " + String(gps.location.lat(), 6) + "\nLon: " + String(gps.location.lng(), 6);
            Blynk.virtualWrite(V_GPS_COORD, coords);
            String quickGPS = String(gps.location.lat(), 4) + ", " + String(gps.location.lng(), 4);
            Blynk.virtualWrite(TEST, quickGPS);
        } else {
            Blynk.virtualWrite(V_GPS_COORD, "Waiting for GPS fix...");
        }
    }
}

// ====================================================================
// SETUP
// ====================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Pin Modes
    pinMode(TEMP_PIN, INPUT);
    pinMode(LDR_PIN, INPUT);
    pinMode(PIR_PIN, INPUT);
    pinMode(RAIN_SENSOR_PIN, INPUT);
    pinMode(LED1_PIN, OUTPUT);
    pinMode(FAN1, OUTPUT);
    pinMode(FAN2, OUTPUT);
    pinMode(FAN3, OUTPUT);
    pinMode(BUZZER, OUTPUT);
    
    // Initial States
    digitalWrite(LED1_PIN, LOW);
    digitalWrite(FAN1, LOW);
    digitalWrite(FAN2, LOW);
    digitalWrite(FAN3, LOW);
    digitalWrite(BUZZER, LOW);
    
    // Servos
    setupRoofServos();
    
    // GPS
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
    
    // Blynk - Using credentials from config.h
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    
    // Time Sync
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
    
    // InfluxDB
    influxClient.validateConnection();
    animalData.addTag("device", "esp32_env");
    
    Serial.println("ESP1 - Environment Controller Started");
    Serial.print("WiFi: ");
    Serial.println(WIFI_SSID);
}

// ====================================================================
// LOOP
// ====================================================================

unsigned long lastRoofCheck = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastInfluxSend = 0;

void loop() {
    Blynk.run();
    unsigned long now = millis();
    
    // GPS Debug
    if (!gpsTested && now > 10000) {
        gpsTested = true;
        int avail = gpsSerial.available();
        if (avail > 0) {
            Blynk.virtualWrite(V_GPS_DEBUG, "GPS Hardware OK - Receiving data");
        } else {
            Blynk.virtualWrite(V_GPS_DEBUG, "ERROR: No GPS data! Check wiring");
        }
    }
    
    sendGpsToBlynk();
    
    // Roof Check
    if (now - lastRoofCheck >= 1000) {
        lastRoofCheck = now;
        checkAndControlRoof();
    }
    
    // Sensor Update
    if (now - lastSensorUpdate >= 2000) {
        lastSensorUpdate = now;
        temperatureC = readTemperature();
        lightValue = readLightLevel();
        motion = readMotion();
        
        Blynk.virtualWrite(V_TEMP, temperatureC);
        Blynk.virtualWrite(V_LIGHT, lightValue);
        Blynk.virtualWrite(V_MOTION, motion ? "Motion Detected" : "No Motion");
    }
    
    // InfluxDB
    if (now - lastInfluxSend >= 10000) {
        lastInfluxSend = now;
        sendDataToInfluxDB();
    }
    
    delay(10);
}
