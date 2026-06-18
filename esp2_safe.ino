// ====================================================================
// ESP2 - Security, Water Management & Mobility
// SAFE VERSION - No hardcoded secrets!
// All secrets are in config.h (NOT uploaded to GitHub)
// ====================================================================

#define BLYNK_PRINT Serial

// ===== Include Configuration (LOCAL ONLY - NOT ON GITHUB) =====
#include "config.h"

// ===== Blynk Setup =====
#define BLYNK_AUTH_TOKEN BLYNK_AUTH_TOKEN_ESP2

// ===== InfluxDB Setup =====
#define INFLUXDB_URL INFLUXDB_URL
#define INFLUXDB_ORG INFLUXDB_ORG
#define INFLUXDB_BUCKET INFLUXDB_BUCKET
#define INFLUXDB_TOKEN INFLUXDB_TOKEN

// ===== Libraries =====
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Arduino.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <WiFiClientSecure.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <TinyGPS++.h>

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
Point securityData("security_system");

// ===== WiFi Credentials (from config.h) =====
char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASSWORD;

// ===== Pin Definitions =====
#define SDA_PIN 8
#define SCL_PIN 9
#define WATER_LEVEL_PIN 10
#define WATER_PUMP_PIN 21

#define ROW1 38
#define ROW2 39
#define ROW3 40
#define ROW4 41
#define COL1 42
#define COL2 45
#define COL3 48

#define SERVO_PIN 13

#define IN1 4
#define IN2 5
#define IN3 7
#define IN4 6
#define ENA 16
#define ENB 17

#define PWM_FREQ 1000
#define PWM_RESOLUTION 8
#define CHANNEL_A 0
#define CHANNEL_B 1

// ===== Blynk Virtual Pins =====
#define VPIN_CHANGE_POSITION V17
#define VPIN_MOTOR_STATUS V13
#define VPIN_GENERATED_CODE V20
#define VPIN_ACCESS_STATUS V21
#define VPIN_DOOR_CONTROL V5
#define VPIN_WATER_LEVEL V19
#define VPIN_WATER_PUMP_BTN V18

// ===== LCD =====
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===== Servo =====
Servo lockServo;
#define SERVO_LOCKED_POS 0
#define SERVO_OPEN_POS 90

// ===== Keypad =====
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
    {'1','2','3'},
    {'4','5','6'},
    {'7','8','9'},
    {'*','0','#'}
};
byte rowPins[ROWS] = {ROW1, ROW2, ROW3, ROW4};
byte colPins[COLS] = {COL1, COL2, COL3};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ===== Global Variables =====
String generatedCode = "";
String enteredCode = "";
bool enteringMode = false;
bool manualPumpControl = false;
int speedValue = 180;
int doorStatus = 0;
int motorStatus = 0;
int accessStatus = 0;
int lastWaterState = -1;
unsigned long lastInfluxSend = 0;
unsigned long lastWaterCheck = 0;

// ====================================================================
// MOTOR FUNCTIONS
// ====================================================================

void stopMotors() {
    ledcWrite(CHANNEL_A, 0);
    ledcWrite(CHANNEL_B, 0);
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
    motorStatus = 0;
    Blynk.virtualWrite(VPIN_MOTOR_STATUS, "Stopped");
}

void moveForward() {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    ledcWrite(CHANNEL_A, 255);
    ledcWrite(CHANNEL_B, 255);
    motorStatus = 1;
    Blynk.virtualWrite(VPIN_MOTOR_STATUS, "Moving Forward");
}

void turnRight() {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    motorStatus = 1;
    ledcWrite(CHANNEL_A, 255);
    ledcWrite(CHANNEL_B, 255);
    Blynk.virtualWrite(VPIN_MOTOR_STATUS, "Turning Right");
}

void Change_Positions() {
    turnRight();
    delay(4000);
    stopMotors();
    delay(1000);
    moveForward();
    delay(3000);
    stopMotors();
}

// ====================================================================
// DOOR ACCESS FUNCTIONS
// ====================================================================

String generate4DigitCode() {
    String code = "";
    for (int i = 0; i < 4; i++) {
        code += String(random(0, 10));
    }
    return code;
}

void showReadyScreen() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Press * to start");
}

void resetSystem() {
    generatedCode = "";
    enteredCode = "";
    enteringMode = false;
    Blynk.virtualWrite(VPIN_GENERATED_CODE, "");
    Blynk.virtualWrite(VPIN_ACCESS_STATUS, "Waiting");
    showReadyScreen();
}

void updateLCDInput() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Code:");
    lcd.setCursor(0, 1);
    lcd.print(enteredCode);
}

void startNewSession() {
    generatedCode = generate4DigitCode();
    enteredCode = "";
    enteringMode = true;
    Serial.print("Generated Code: ");
    Serial.println(generatedCode);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Code:");
    Blynk.virtualWrite(VPIN_GENERATED_CODE, generatedCode);
    Blynk.virtualWrite(VPIN_ACCESS_STATUS, "Code Sent");
}

void SERVO_CONTROL() {
    lockServo.write(SERVO_OPEN_POS);
    delay(2000);
    lockServo.write(SERVO_LOCKED_POS);
    delay(2000);
}

void Access_granted() {
    Serial.println("Access Granted");
    Blynk.virtualWrite(VPIN_ACCESS_STATUS, "Correct");
    accessStatus = 1;
    doorStatus = 1;
    lcd.clear();
    lcd.setCursor(0, 0);
    SERVO_CONTROL();
    delay(1000);
    resetSystem();
}

void Access_denied() {
    Serial.println("Access Denied");
    Blynk.virtualWrite(VPIN_ACCESS_STATUS, "Wrong");
    accessStatus = 0;
    enteredCode = "";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wrong! Try again");
    delay(1500);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Code:");
}

// ====================================================================
// WATER MANAGEMENT FUNCTIONS
// ====================================================================

void pumpOn() {
    digitalWrite(WATER_PUMP_PIN, LOW);
}

void pumpOff() {
    digitalWrite(WATER_PUMP_PIN, HIGH);
}

void checkWaterLevel() {
    int waterState = digitalRead(WATER_LEVEL_PIN);
    String currentMessage;
    
    if (waterState != lastWaterState) {
        if (waterState == LOW) {
            currentMessage = "Water Detected";
            Serial.println("Water Level: Water Detected");
            Blynk.virtualWrite(VPIN_WATER_LEVEL, currentMessage);
            if (!manualPumpControl) {
                pumpOff();
                Blynk.virtualWrite(VPIN_WATER_PUMP_BTN, 0);
            }
        } else {
            currentMessage = "No Water";
            Serial.println("Water Level: No Water");
            Blynk.virtualWrite(VPIN_WATER_LEVEL, currentMessage);
            if (!manualPumpControl) {
                pumpOn();
                Blynk.virtualWrite(VPIN_WATER_PUMP_BTN, 1);
            }
        }
        lastWaterState = waterState;
    }
}

// ====================================================================
// BLYNK WIDGET HANDLERS
// ====================================================================

BLYNK_WRITE(VPIN_CHANGE_POSITION) {
    int buttonState = param.asInt();
    if (buttonState == 1) {
        Change_Positions();
        Blynk.virtualWrite(VPIN_CHANGE_POSITION, 0);
    }
}

BLYNK_WRITE(VPIN_WATER_PUMP_BTN) {
    int state = param.asInt();
    manualPumpControl = true;
    if (state == 1) {
        pumpOn();
    } else {
        pumpOff();
    }
}

BLYNK_WRITE(VPIN_DOOR_CONTROL) {
    int state = param.asInt();
    if (state == 1) {
        lockServo.write(SERVO_OPEN_POS);
        doorStatus = 1;
        Serial.println("Door Opened");
    } else {
        lockServo.write(SERVO_LOCKED_POS);
        doorStatus = 0;
        Serial.println("Door Closed");
    }
}

// ====================================================================
// INFLUXDB FUNCTIONS
// ====================================================================

void sendDataToInfluxDB() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("InfluxDB: WiFi not connected");
        return;
    }
    
    int waterState = digitalRead(WATER_LEVEL_PIN);
    int pumpState = digitalRead(WATER_PUMP_PIN) == LOW ? 1 : 0;
    
    securityData.clearFields();
    securityData.addField("water_level", waterState);
    securityData.addField("pump", pumpState);
    securityData.addField("door", doorStatus);
    securityData.addField("motor", motorStatus);
    securityData.addField("access", accessStatus);
    
    if (influxClient.writePoint(securityData)) {
        Serial.println("InfluxDB write successful");
    } else {
        Serial.print("InfluxDB write failed: ");
        Serial.println(influxClient.getLastErrorMessage());
    }
}

// ====================================================================
// SETUP
// ====================================================================

void setup() {
    Serial.begin(115200);
    randomSeed(micros());
    
    // LCD
    Wire.begin(SDA_PIN, SCL_PIN);
    lcd.init();
    lcd.backlight();
    
    // Water Level & Pump
    pinMode(WATER_LEVEL_PIN, INPUT);
    pinMode(WATER_PUMP_PIN, OUTPUT);
    digitalWrite(WATER_PUMP_PIN, HIGH);
    
    // Motor Pins
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);
    pinMode(ENA, OUTPUT);
    pinMode(ENB, OUTPUT);
    
    // Motor PWM Setup
    ledcSetup(CHANNEL_A, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(ENA, CHANNEL_A);
    ledcAttachPin(ENB, CHANNEL_B);
    
    stopMotors();
    
    // Door Servo
    lockServo.setPeriodHertz(50);
    lockServo.attach(SERVO_PIN, 500, 2400);
    lockServo.write(SERVO_LOCKED_POS);
    
    // Blynk - Using credentials from config.h
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    
    // LCD Ready
    showReadyScreen();
    
    // Blynk Initial States
    Blynk.virtualWrite(VPIN_GENERATED_CODE, "");
    Blynk.virtualWrite(VPIN_ACCESS_STATUS, "Waiting");
    Blynk.virtualWrite(VPIN_MOTOR_STATUS, "Stopped");
    Blynk.virtualWrite(VPIN_WATER_PUMP_BTN, 0);
    
    // Water Check
    checkWaterLevel();
    
    // Time Sync
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
    
    // InfluxDB
    if (influxClient.validateConnection()) {
        Serial.print("Connected to InfluxDB: ");
        Serial.println(influxClient.getServerUrl());
    } else {
        Serial.print("InfluxDB connection failed: ");
        Serial.println(influxClient.getLastErrorMessage());
    }
    
    Serial.println("ESP2 - Security & Water Controller Started");
    Serial.print("WiFi: ");
    Serial.println(WIFI_SSID);
}

// ====================================================================
// LOOP
// ====================================================================

void loop() {
    Blynk.run();
    
    // Water Level Check
    if (!manualPumpControl && millis() - lastWaterCheck >= 1000) {
        lastWaterCheck = millis();
        checkWaterLevel();
    }
    
    // InfluxDB
    if (millis() - lastInfluxSend >= 10000) {
        lastInfluxSend = millis();
        sendDataToInfluxDB();
    }
    
    // Keypad Handling
    char key = keypad.getKey();
    if (!key) {
        return;
    }
    
    Serial.print("Pressed: ");
    Serial.println(key);
    
    if (!enteringMode) {
        if (key == '*') {
            startNewSession();
        }
        return;
    }
    
    if (key >= '0' && key <= '9') {
        if (enteredCode.length() < 4) {
            enteredCode += key;
            updateLCDInput();
        }
    }
    else if (key == '#') {
        if (enteredCode == generatedCode) {
            Access_granted();
        } else {
            Access_denied();
        }
    }
}
