#include <WiFi.h>
#include <HTTPClient.h>
#include "Plantower_PMS7003.h"
#include <ErriezMHZ19B.h>
#include "time.h"

// ==========================================
// WIFI & GOOGLE SHEETS SETTINGS
// ==========================================
const char* ssid = "BigB's Hotspot";
const char* password = "yayayaya";

#define SHEET_URL  "https://script.google.com/macros/s/AKfycbw0ckeyRHvZLNvdhsj9sUhP6AFJzb8RCe7T-Ch9kp_h9wN9IxMHT-s096erWL8HQXHnew/exec"
#define GROUP      "1" 
#define DEVICE_ID  "ESP_MPSTME"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800; 
const int   daylightOffset_sec = 0; 

#define MODEM_POWER_ON 23
#define MODEM_PWKEY    4

#define PMS_RX 32
#define PMS_TX 33
#define MS_RX  19
#define MS_TX  18
#define MHZ_RX 22
#define MHZ_TX 21

Plantower_PMS7003 pms7003 = Plantower_PMS7003();
ErriezMHZ19B mhz19b(&Serial2);

struct SensorData {
  String lux;
  String pressure;
  String temp;
  String humidity;
};

SensorData sensor_data;
char output[256];
unsigned long lastReport = 0;
const unsigned long REPORT_INTERVAL = 10000; 
int pm25 = 0;

// ==========================================
// UPDATED HELPER FUNCTIONS
// ==========================================
void printTS() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.print(F("[TIME SYNCING...] "));
    return;
  }
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "[%d-%m-%Y][%H:%M:%S] ", &timeinfo);
  Serial.print(timeStringBuff);
}

void parseSensorString(const String &input, SensorData &data) {
  int idx1 = input.indexOf('#');
  int idx2 = input.indexOf('#', idx1 + 1);
  int idx3 = input.indexOf('#', idx2 + 1);

  if (idx1 == -1 || idx2 == -1 || idx3 == -1) return;

  data.lux      = input.substring(0, idx1);
  data.pressure = input.substring(idx1 + 1, idx2);
  data.temp     = input.substring(idx2 + 1, idx3);
  data.humidity = input.substring(idx3 + 1);
}

SensorData readMobileSense() {
  SensorData data;
  String input = "";
  bool started = false;

  Serial2.end();
  Serial2.begin(115200, SERIAL_8N1, MS_RX, MS_TX);
  Serial2.write('1');
  Serial2.write(0xD);
  Serial2.write(0xA);
  delay(300);

  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '*') {
      input = "";
      started = true;
    } else if (c == '@' && started) {
      parseSensorString(input, data);
      started = false;
      break;
    } else if (started) {
      input += c;
    }
  }
  Serial2.end();
  Serial2.begin(9600, SERIAL_8N1, MHZ_RX, MHZ_TX);
  return data;
}

void pushToCloud(int pm, String temp, String hum, String lux, int co2, String press) {
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    String url = String(SHEET_URL)
      + "?group="       + GROUP
      + "&temperature=" + temp
      + "&humidity="    + hum
      + "&light="       + lux
      + "&co2="         + String(co2)
      + "&pm25="        + String(pm)
      + "&deviceId="    + DEVICE_ID
      + "&pressure="    + press;
    
    printTS(); Serial.println(F("[Cloud] Sending via Wi-Fi: To Google Sheet"));
    http.begin(url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      printTS(); Serial.println("[Cloud] Response: " + http.getString());
    } else {
      printTS(); Serial.println("[Cloud] Request failed. Error: " + String(httpResponseCode));
    }
    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(MODEM_POWER_ON, OUTPUT);
  pinMode(MODEM_PWKEY, OUTPUT);
  digitalWrite(MODEM_POWER_ON, HIGH);
  digitalWrite(MODEM_PWKEY, HIGH); 

  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  Serial1.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
  pms7003.init(&Serial1);
  Serial2.begin(9600, SERIAL_8N1, MHZ_RX, MHZ_TX);

  while (!mhz19b.detect()) { delay(2000); }
  while (mhz19b.isWarmingUp()) { printTS(); Serial.println(F("MH-Z19B Warming up...")); delay(5000); }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) { WiFi.begin(ssid, password); }
  pms7003.updateFrame();
  if (pms7003.hasNewData()) { pm25 = pms7003.getPM_2_5(); }

  if (millis() - lastReport >= REPORT_INTERVAL) {
    lastReport = millis();
    sensor_data = readMobileSense();
    int16_t co2 = mhz19b.readCO2();

    Serial.println(); 
    printTS(); Serial.println(F("--- Local Sensor Readings ---"));
    sprintf(output, "Temp: %s C | Hum: %s %% | Lux: %s lx | CO2: %d ppm | PM2.5: %d ug/m3 | Pressure: %s hPa", 
            sensor_data.temp.c_str(), sensor_data.humidity.c_str(), sensor_data.lux.c_str(), co2, pm25, sensor_data.pressure.c_str());
    printTS(); Serial.println(output);

    pushToCloud(pm25, sensor_data.temp, sensor_data.humidity, sensor_data.lux, co2, sensor_data.pressure);
  }
}