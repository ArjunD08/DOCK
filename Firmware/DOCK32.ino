#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define SDA_PIN 8
#define SCL_PIN 9

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= BUTTONS =================
#define BTN_UP      0   // INPUT_PULLDOWN → HIGH
#define BTN_DOWN    1   // INPUT_PULLUP → LOW
#define BTN_SELECT  2   // INPUT_PULLUP → LOW (reserved)

// ================= ADDRESS PINS =================
#define ADDR_A 6   // NEW LOGIC
#define ADDR_B 7

// ================= SHARED SENSOR PINS =================
#define DHTPIN   3
#define VIB_PIN  5
#define LDR_PIN  10   // DIGITAL LDR

// ================= BUZZER =================
#define BUZZER_1 5
#define PWM_FREQ 200
#define PWM_RES 8

// ================= MODES =================
enum Mode {
  MODE_MENU,
  MODE_SLEEP_POD,
  MODE_HOME_AUTO
};

Mode currentMode = MODE_MENU;

// ================= DHT =================
#define DHTTYPE DHT22
DHT* dht = nullptr;

float temperature = 0;
float humidity = 0;

// ================= SLEEP POD =================
int noiseAmplitude = 40;

// ================= HOME AUTO =================
bool vibrationDetected = false;
bool isDark = false;
bool windStorm = false;

// ================= MENU =================
const char* menuItems[] = {
  "CALL",
  "MAP",
  "GAMES"
};
const int menuCount = 3;
int selectedIndex = 0;

// ================= TIMING =================
unsigned long lastButtonTime = 0;
unsigned long lastSensorTime = 0;
unsigned long lastNoiseTime = 0;
const unsigned long debounceDelay = 200;

// ================= SPLASH =================
void showSplash(const char* text) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 24);
  display.print(text);
  display.display();
  delay(1500);
}

// ================= MODE MANAGEMENT =================
void stopAllPeripherals() {
  ledcWrite(BUZZER_1, 0);
  if (dht) {
    delete dht;
    dht = nullptr;
  }
}

void enterSleepPod() {
  stopAllPeripherals();
  dht = new DHT(DHTPIN, DHTTYPE);
  dht->begin();
  ledcAttach(BUZZER_1, PWM_FREQ, PWM_RES);
  currentMode = MODE_SLEEP_POD;
}

void enterHomeAuto() {
  stopAllPeripherals();
  dht = new DHT(DHTPIN, DHTTYPE);
  dht->begin();
  pinMode(VIB_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  currentMode = MODE_HOME_AUTO;
}

void enterMenu() {
  stopAllPeripherals();
  currentMode = MODE_MENU;
}

// ================= ADDRESS CHECK =================
void checkAddressPins() {
  bool a = digitalRead(ADDR_A);
  bool b = digitalRead(ADDR_B);

  if (a == HIGH && b == LOW) {
    if (currentMode != MODE_SLEEP_POD) enterSleepPod();
  }
  else if (a == LOW && b == HIGH) {
    if (currentMode != MODE_HOME_AUTO) enterHomeAuto();
  }
  else {
    if (currentMode != MODE_MENU) enterMenu();
  }
}

// ================= MENU =================
void drawMenu() {
  display.clearDisplay();
  display.setTextSize(2);

  for (int i = 0; i < menuCount; i++) {
    display.setCursor(8, i * 20);
    display.print(menuItems[i]);
    if (i == selectedIndex) {
      display.setCursor(110, i * 20);
      display.print(">");
    }
  }
  display.display();
}

// ================= SLEEP POD =================
void drawSleepPod() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("SLEEP POD");

  display.setTextSize(1);
  display.setCursor(0, 28);
  display.print("AMP: ");
  display.print(noiseAmplitude);

  display.setCursor(0, 44);
  display.print("T:");
  display.print(temperature, 1);
  display.print("C H:");
  display.print(humidity, 0);
  display.print("%");

  display.display();
}

// ================= HOME AUTO =================
void drawHomeAuto() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("HOME AUTO");

  display.setTextSize(1);
  display.setCursor(0, 28);
  display.print("T:");
  display.print(temperature, 1);
  display.print("C  H:");
  display.print(humidity, 0);
  display.print("%");

  display.setCursor(0, 48);
  display.print(windStorm ? "WIND STORM!" : "NORMAL");

  display.display();
}

// ================= BUTTON HANDLER =================
void handleButtons() {
  if (currentMode != MODE_MENU) return;
  if (millis() - lastButtonTime < debounceDelay) return;

  if (digitalRead(BTN_UP) == HIGH) {
    selectedIndex = (selectedIndex - 1 + menuCount) % menuCount;
    lastButtonTime = millis();
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    selectedIndex = (selectedIndex + 1) % menuCount;
    lastButtonTime = millis();
  }
}

// ================= UPDATES =================
void updateSensors() {
  if (!dht || millis() - lastSensorTime < 2000) return;

  temperature = dht->readTemperature();
  humidity = dht->readHumidity();

  if (currentMode == MODE_HOME_AUTO) {
    vibrationDetected = digitalRead(VIB_PIN);
    isDark = (digitalRead(LDR_PIN) == LOW);
    windStorm = vibrationDetected && isDark;
  }

  lastSensorTime = millis();
}

void updateWhiteNoise() {
  if (currentMode == MODE_SLEEP_POD && millis() - lastNoiseTime > 5) {
    int duty = random(0, noiseAmplitude);
    ledcWrite(BUZZER_1, duty);
    lastNoiseTime = millis();
  }
}

// ================= SETUP =================
void setup() {
  pinMode(BTN_UP, INPUT_PULLDOWN);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  pinMode(ADDR_A, INPUT_PULLUP);
  pinMode(ADDR_B, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);

  showSplash("DOCK32");
}

// ================= LOOP =================
void loop() {
  checkAddressPins();
  handleButtons();
  updateSensors();
  updateWhiteNoise();

  if (currentMode == MODE_MENU) drawMenu();
  else if (currentMode == MODE_SLEEP_POD) drawSleepPod();
  else if (currentMode == MODE_HOME_AUTO) drawHomeAuto();
}
