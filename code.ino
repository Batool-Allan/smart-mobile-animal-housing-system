ESP1 CODE:
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6YKdWBVUb"
#define BLYNK_TEMPLATE_NAME "Animal Housing System"
#define BLYNK_AUTH_TOKEN "mSd_6qYXKe0pSoOd3PN4OrxVMzqj5Yn9"
#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_ORG "animal housing"
#define INFLUXDB_BUCKET "animal_housing"
#define INFLUXDB_TOKEN
"52hrfzPRKW_4T5_wN4WXoA4N6lxGLfGzmoHiB09c1Gud4HEhe0r3ZXNyqb9joYp8
dwCjTwFWr3W56HZtezlTAw=="
#include <WiFiClientSecure.h>
#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <TinyGPS++.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#define TZ_INFO "EET-2EEST,M3.5.5/0,M10.5.5/1"
InfluxDBClient influxClient(
INFLUXDB_URL,
INFLUXDB_ORG,
INFLUXDB_BUCKET,
INFLUXDB_TOKEN,
InfluxDbCloud2CACert
);
Point animalData("animal_house");
char ssid[] = "HUAWEI-2.4G-jNmV";
char pass[] = "96JwMPDs";
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
Servo roofServo;
Servo roofServo2;
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;
#define SERVO_OPEN_ANGLE 90
#define SERVO_CLOSED_ANGLE 0
#define SERVO2_OPEN_ANGLE 180
#define SERVO2_CLOSED_ANGLE 90
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
void OpenRoof() {
if (roofIsOpen) {
return;
}
roofServo.write(SERVO_OPEN_ANGLE);
roofServo2.write(SERVO2_OPEN_ANGLE);
delay(500);
roofIsOpen = true;
Blynk.virtualWrite(V_ROOF_STATUS, "Roof Open");
}
void CloseRoof() {
if (!roofIsOpen) {
return;
}
roofServo.write(SERVO_CLOSED_ANGLE);
roofServo2.write(SERVO2_CLOSED_ANGLE);
delay(500);
roofIsOpen = false;
Blynk.virtualWrite(V_ROOF_STATUS, "Roof Closed");
}
void setupRoofServos() {
if (roofServo.attached()) {
roofServo.detach();
delay(100);
}
if (roofServo2.attached()) {
roofServo2.detach();
delay(100);
}
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
if (value == 1) {
OpenRoof();
}
}
BLYNK_WRITE(V_ROOF_CLOSE) {
int value = param.asInt();
if (value == 1) {
CloseRoof();
}
}
float readTemperature() {
int raw = analogRead(TEMP_PIN);
float voltage = raw * (3.3f / 4095.0f);
temperatureC = voltage * 100.0f;
if (temperatureC > 35 && !tempAlertSent) {
Blynk.logEvent("temp_alert", "High temperature detected!");
tempAlertSent = true;
}
if (temperatureC <= 35) {
tempAlertSent = false;
}
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
void sendDataToInfluxDB() {
if (WiFi.status() != WL_CONNECTED) {
return;
}
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
unsigned long lastGpsSend = 0;
unsigned long lastGpsDebug = 0;
bool gpsTested = false;
unsigned long gpsTestTime = 0;
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
double lat = gps.location.lat();
double lng = gps.location.lng();
Blynk.virtualWrite(V8,
gps.location.lat(),
gps.location.lng());
String coords = "Lat: " + String(lat, 6) + "\nLon: " + String(lng, 6);
Blynk.virtualWrite(V_GPS_COORD, coords);
String quickGPS = String(lat, 4) + ", " + String(lng, 4);
Blynk.virtualWrite(TEST, quickGPS);
} else {
Blynk.virtualWrite(V_GPS_COORD, "Waiting for GPS fix...");
}
}
}
void setup() {
Serial.begin(115200);
delay(1000);
pinMode(TEMP_PIN, INPUT);
pinMode(LDR_PIN, INPUT);
pinMode(PIR_PIN, INPUT);
pinMode(RAIN_SENSOR_PIN, INPUT);
pinMode(LED1_PIN, OUTPUT);
pinMode(FAN1, OUTPUT);
pinMode(FAN2, OUTPUT);
pinMode(FAN3, OUTPUT);
pinMode(BUZZER, OUTPUT);
digitalWrite(LED1_PIN, LOW);
digitalWrite(FAN1, LOW);
digitalWrite(FAN2, LOW);
digitalWrite(FAN3, LOW);
digitalWrite(BUZZER, LOW);
setupRoofServos();
gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
influxClient.validateConnection();
animalData.addTag("device", "esp32_env");
}
unsigned long lastRoofCheck = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastInfluxSend = 0;
void loop() {
Blynk.run();
unsigned long now = millis();
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
if (now - lastRoofCheck >= 1000) {
lastRoofCheck = now;
checkAndControlRoof();
}
if (now - lastSensorUpdate >= 2000) {
lastSensorUpdate = now;
temperatureC = readTemperature();
lightValue = readLightLevel();
motion = readMotion();
Blynk.virtualWrite(V_TEMP, temperatureC);
Blynk.virtualWrite(V_LIGHT, lightValue);
Blynk.virtualWrite(V_MOTION, motion ? "Motion Detected" : "No Motion");
}
if (now - lastInfluxSend >= 10000) {
lastInfluxSend = now;
sendDataToInfluxDB();
}
delay(10);
}
ESP2 CODE:
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6YKdWBVUb"
#define BLYNK_TEMPLATE_NAME "Animal Housing System"
#define BLYNK_AUTH_TOKEN "rcG8lSCgMqZmGT3iAGoHuSmgVkUag-TI"
#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_ORG "animal housing"
#define INFLUXDB_BUCKET "animal_housing"
#define INFLUXDB_TOKEN
"52hrfzPRKW_4T5_wN4WXoA4N6lxGLfGzmoHiB09c1Gud4HEhe0r3ZXNyqb9joYp8dwCjTwFWr3W56HZt
ezlTAw=="
#define TZ_INFO "EET-2EEST,M3.5.5/0,M10.5.5/1"
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
InfluxDBClient influxClient(
INFLUXDB_URL,
INFLUXDB_ORG,
INFLUXDB_BUCKET,
INFLUXDB_TOKEN,
InfluxDbCloud2CACert
);
Point securityData("security_system");
char ssid[] = "HUAWEI-2.4G-jNmV";
char pass[] = "96JwMPDs";
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
#define VPIN_CHANGE_POSITION V17
#define VPIN_MOTOR_STATUS V13
#define VPIN_GENERATED_CODE V20
#define VPIN_ACCESS_STATUS V21
#define VPIN_DOOR_CONTROL V5
#define VPIN_WATER_LEVEL V19
#define VPIN_WATER_PUMP_BTN V18
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo lockServo;
#define SERVO_LOCKED_POS 0
#define SERVO_OPEN_POS 90
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
String lastWaterMessage = "";
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
void setup() {
Serial.begin(115200);
randomSeed(micros());
Wire.begin(SDA_PIN, SCL_PIN);
lcd.init();
lcd.backlight();
pinMode(WATER_LEVEL_PIN, INPUT);
pinMode(WATER_PUMP_PIN, OUTPUT);
digitalWrite(WATER_PUMP_PIN, HIGH);
pinMode(IN1, OUTPUT);
pinMode(IN2, OUTPUT);
pinMode(IN3, OUTPUT);
pinMode(IN4, OUTPUT);
pinMode(ENA, OUTPUT);
pinMode(ENB, OUTPUT);
digitalWrite(IN1, HIGH);
digitalWrite(IN2, LOW);
ledcSetup(CHANNEL_A, PWM_FREQ, PWM_RESOLUTION);
ledcSetup(CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);
ledcAttachPin(ENA, CHANNEL_A);
ledcAttachPin(ENB, CHANNEL_B);
stopMotors();
lockServo.setPeriodHertz(50);
lockServo.attach(SERVO_PIN, 500, 2400);
lockServo.write(SERVO_LOCKED_POS);
pinMode(IN1, OUTPUT);
pinMode(IN2, OUTPUT);
pinMode(IN3, OUTPUT);
pinMode(IN4, OUTPUT);
pinMode(ENA, OUTPUT);
pinMode(ENB, OUTPUT);
digitalWrite(IN1, LOW);
digitalWrite(IN2, LOW);
digitalWrite(IN3, LOW);
digitalWrite(IN4, LOW);
ledcSetup(CHANNEL_A, 5000, 8);
ledcSetup(CHANNEL_B, 5000, 8);
ledcAttachPin(ENA, CHANNEL_A);
ledcAttachPin(ENB, CHANNEL_B);
stopMotors();
Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
showReadyScreen();
Blynk.virtualWrite(VPIN_GENERATED_CODE, "");
Blynk.virtualWrite(VPIN_ACCESS_STATUS, "Waiting");
Blynk.virtualWrite(VPIN_MOTOR_STATUS, "Stopped");
Blynk.virtualWrite(VPIN_WATER_PUMP_BTN, 0);
checkWaterLevel();
timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
if (influxClient.validateConnection()) {
Serial.print("Connected to InfluxDB: ");
Serial.println(influxClient.getServerUrl());
} else {
Serial.print("InfluxDB connection failed: ");
Serial.println(influxClient.getLastErrorMessage());
}
}
void loop() {
Blynk.run();
if (!manualPumpControl && millis() - lastWaterCheck >= 1000) {
lastWaterCheck = millis();
checkWaterLevel();
}
if (millis() - lastInfluxSend >= 10000) {
lastInfluxSend = millis();
sendDataToInfluxDB();
}
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