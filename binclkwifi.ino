


#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <WiFiManager.h>

// Add these pin definitions at the top
#define TRIGGER_PIN D3
#define BUTTON_SELECT D1
#define BUTTON_INC D2
#define BUTTON_DEC D6

// Add debounce variables
unsigned long lastDebounceTime = 0;
#define DEBOUNCE_DELAY 50
int lastSelectState = HIGH;
int lastIncState = HIGH;
int lastDecState = HIGH;

// Add time setting variables
enum SetMode { NORMAL_MODE, SET_HOURS, SET_MINUTES, SET_SECONDS };
SetMode currentMode = NORMAL_MODE;
unsigned long manualEpoch = 0;
bool timeAdjusted = false;

unsigned int localPort = 123;  // local port to listen for UDP packets
IPAddress timeServerIP;
const char* ntpServerName = "in.pool.ntp.org";
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];
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
WiFiManager wm;
WiFiManagerParameter custom_field;

// Brightness control
void bright() {
  short pin = A0;
  int va = analogRead(pin);
  short fadeAmount = 5;
  int bri = 255 - va;
  pinMode(2, OUTPUT);
  pinMode(pin, INPUT);
  digitalWrite(2, LOW);

  if (bri >= 0 || bri <= 255) {
    fadeAmount = -fadeAmount;
  }
  bri = +fadeAmount;

  analogWrite(2, bri);
}

/* 
 *  This section bit shifts the time byte 
 */
byte getTimeByte(int timePart) {
  int tenPart = timePart / 10;
  int onePart = timePart % 10;
  Serial.println(tenPart);
    Serial.println(onePart);

  return (onePart |=( tenPart << 4));
}


// Button handling for time setting
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

// Modify tick function to use manualEpoch in set mode
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

// Add WiFi reconnect logic to loop
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

// WiFi configuration button
void checkButton() {
  if (digitalRead(TRIGGER_PIN) == LOW) {
    delay(50);
    if (digitalRead(TRIGGER_PIN) == LOW) {
      delay(3000);
      if (digitalRead(TRIGGER_PIN) == LOW) {
        wm.resetSettings();
        ESP.restart();
      }
      wm.setConfigPortalTimeout(120);
      if (!wm.startConfigPortal("OnDemandAP", "password")) {
        delay(3000);
      }
    }
  }
}

// WiFiManager parameter setup
void saveParamCallback() {
  Serial.println("Parameters saved");
}

// Function to sync time from NTP server

/* 
 *  The following code is based on the NTP Client example for ESP8266, more info on:
 *  https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WiFi/examples/NTPClient
 */
void getDateTime() {

  disableLeds();
  bool timeSuccess = false;
  ticker.detach();

  while (!timeSuccess) {
  
    //get a random server from the pool
    WiFi.hostByName(ntpServerName, timeServerIP); 
  
    sendNTPpacket(timeServerIP); // send an NTP packet to a time server
    // wait to see if a reply is available
    delay(1000);
    
    int cb = udp.parsePacket();
    if (!cb) {
      Serial.println("no packet yet");
    }
    else {      
      Serial.print("packet received, length=");
      Serial.println(cb);
      // We've received a packet, read the data from it
      udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  
      //the timestamp starts at byte 40 of the received packet and is four bytes,
      // or two words, long. First, esxtract the two words:
  
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      Serial.print("Seconds since Jan 1 1900 = " );
      Serial.println(secsSince1900);
  
      // now convert NTP time into everyday time:
      Serial.print("Unix time = ");
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
      const unsigned long seventyYears = 2208988800UL;
      // subtract seventy years:
      epoch =( secsSince1900 - seventyYears);
      // print Unix time:
      Serial.println(epoch);  
  
      // print the hour, minute and second:
      Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
      Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
      Serial.print(':');
      if ( ((epoch % 3600) / 60) < 10 ) {
        // In the first 10 minutes of each hour, we'll want a leading '0'
        Serial.print('0');
      }
      Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
      Serial.print(':');
      if ( (epoch % 60) < 10 ) {
        // In the first 10 seconds of each minute, we'll want a leading '0'
        Serial.print('0');
      }
      Serial.println(epoch % 60); // print the second 

      timeSuccess = true;      

      // Enable ticker every second
      ticker.attach(1, tick);
    }
    // wait x seconds before asking for the time again
    delay(1000);
  }  
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

// Main setup function
void setup() {
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_INC, INPUT_PULLUP);
  pinMode(BUTTON_DEC, INPUT_PULLUP);
  pinMode(TRIGGER_PIN, INPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  WiFi.mode(WIFI_STA);
  wm.setConfigPortalTimeout(35);
  wm.autoConnect("AutoConnectAP", "password");

  udp.begin(localPort);
  getDateTime();
  ticker.attach(1, tick);
}

// Main loop function
void loop() {
  checkButton();
  handleButtons();
  checkWiFi();
}
