#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <WiFiManager.h>

#define TRIGGER_PIN D3
#define BUTTON_SELECT D1
#define BUTTON_INC D2
#define BUTTON_DEC D6

unsigned int localPort = 123;  // local port to listen for UDP packets

IPAddress timeServerIP;         // time.nist.gov NTP server address
const char* ntpServerName = "in.pool.ntp.org";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP udp;

// Pin connected to ST_CP of 74HC595
int latchPin = 4;
// Pin connected to SH_CP of 74HC595
int clockPin = 5;
// Pin connected to DS of 74HC595
int dataPin = 14;

unsigned long epoch;

Ticker ticker;
Ticker tickerRefresh;

Ticker brightness; 
double timeZoneCorrection = 5.30;
const int rebootEvery = 60 * 60 * 24;

bool wm_nonblocking = false; // change to true to use non blocking

WiFiManager wm; // global wm instance
WiFiManagerParameter custom_field; // global param ( for non blocking w params )

// Debounce variables
unsigned long lastDebounceTime = 0;
#define DEBOUNCE_DELAY 50
int lastSelectState = HIGH;
int lastIncState = HIGH;
int lastDecState = HIGH;

// Time setting variables
enum SetMode { NORMAL_MODE, SET_HOURS, SET_MINUTES, SET_SECONDS };
SetMode currentMode = NORMAL_MODE;
unsigned long manualEpoch = 0;
bool timeAdjusted = false;

void bright() {
  short pin = A0; pin = pin;
  int va = analogRead(pin);
  short fadeAmount = 5;
  int bri = 255 - va;
  pinMode(2, OUTPUT);
  pinMode(pin, INPUT);
  digitalWrite(2, LOW);

  // reverse the direction of the fading at the ends of the fade:
  if (bri >= 0 || bri <= 255) {
    fadeAmount = -fadeAmount;
  }
  bri = +fadeAmount;

  analogWrite(2, bri);
}

byte getTimeByte(int timePart) {
  int tenPart = timePart / 10;
  int onePart = timePart % 10;
  Serial.println(tenPart);
    Serial.println(onePart);

  return (onePart |=( tenPart << 4));
}

void handleButtons() {
  int selectState = digitalRead(BUTTON_SELECT);
  int incState = digitalRead(BUTTON_INC);
  int decState = digitalRead(BUTTON_DEC);

  // Debounce logic for select button
  if (selectState != lastSelectState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (selectState == LOW) {
      switch (currentMode) {
        case NORMAL_MODE:
          manualEpoch = epoch + 19800; // Convert to local time
          currentMode = SET_HOURS;
          ticker.detach();
          break;
        case SET_HOURS:
          currentMode = SET_MINUTES;
          break;
        case SET_MINUTES:
          currentMode = SET_SECONDS;
          break;
        case SET_SECONDS:
          // Convert back to UTC and normalize
          epoch = (manualEpoch - 19800) % 86400;
          currentMode = NORMAL_MODE;
          ticker.attach(1, tick);
          timeAdjusted = true;
          break;
      }
      while (digitalRead(BUTTON_SELECT) == LOW); // Wait for release
    }
  }
  lastSelectState = selectState;

  // Handle increment/decrement in set mode
  if (currentMode != NORMAL_MODE) {
    handleAdjustments(incState, decState);
  }
}

void handleAdjustments(int incState, int decState) {
  static unsigned long lastAdjust = 0;
  if (millis() - lastAdjust < 200) return; // Rate limit

  int hours = manualEpoch / 3600 % 24;
  int minutes = (manualEpoch % 3600) / 60;
  int seconds = manualEpoch % 60;

  if (incState == LOW) {
    switch (currentMode) {
      case SET_HOURS: hours = (hours + 1) % 24; break;
      case SET_MINUTES: minutes = (minutes + 1) % 60; break;
      case SET_SECONDS: seconds = (seconds + 1) % 60; break;
    }
    lastAdjust = millis();
  }

  if (decState == LOW) {
    switch (currentMode) {
      case SET_HOURS: hours = (hours - 1 + 24) % 24; break;
      case SET_MINUTES: minutes = (minutes - 1 + 60) % 60; break;
      case SET_SECONDS: seconds = (seconds - 1 + 60) % 60; break;
    }
    lastAdjust = millis();
  }

  manualEpoch = hours * 3600 + minutes * 60 + seconds;
}

void tick() {
  if (currentMode == NORMAL_MODE) {
    epoch++;
  }

  int hours = ((epoch + 19800) % 86400L) / 3600;
  int minutes = ((epoch + 19800) % 3600) / 60;
  int seconds = (epoch + 19800) % 60;

  if (currentMode != NORMAL_MODE) {
    hours = manualEpoch / 3600 % 24;
    minutes = (manualEpoch % 3600) / 60;
    seconds = manualEpoch % 60;
  }

  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, getTimeByte(seconds));
  shiftOut(dataPin, clockPin, MSBFIRST, getTimeByte(minutes));
  shiftOut(dataPin, clockPin, MSBFIRST, getTimeByte(hours));
  digitalWrite(latchPin, HIGH);
}

void checkWiFi() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 30000) return; // Check every 30 seconds

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      getDateTime(); // Sync time when reconnected
      timeAdjusted = false;
    }
  }
  lastCheck = millis();
}

void setup() {
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_INC, INPUT_PULLUP);
  pinMode(BUTTON_DEC, INPUT_PULLUP);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  disableLeds();
  delay(300);
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  WiFi.mode(WIFI_STA);
  delay(3000);
  Serial.println("\n Starting");

  pinMode(TRIGGER_PIN, INPUT);
  if (wm_nonblocking) wm.setConfigPortalBlocking(false);

  wm.setConfigPortalTimeout(35);
  bool res;

  res = wm.autoConnect("AutoConnectAP", "password");
  if (!res) {
    Serial.println("Failed to connect or hit timeout");
  } else {
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }

  udp.begin(localPort);

  getDateTime();
  tickerRefresh.attach(rebootEvery, reboot);
}

void loop() {
  checkButton();
  handleButtons();
  checkWiFi();

  if (WiFi.status() == WL_CONNECTED && !timeAdjusted) {
    getDateTime();
  }
}
