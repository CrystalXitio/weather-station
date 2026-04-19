#define TINY_GSM_MODEM_SIM800 

#include <SoftwareSerial.h>
#include <TinyGsmClient.h>
#include <HTTPClient.h>
#include "Plantower_PMS7003.h"
#include <ErriezMHZ19B.h>
#include <time.h>
#include <sys/time.h>

// ==========================================
// CELLULAR & GOOGLE SHEETS SETTINGS
// ==========================================
const char apn[]      = "airtelgprs.com";
const char gprsUser[] = "user id";
const char gprsPass[] = "password";

#define SHEET_URL  "https://script.google.com/macros/s/AKfycbw0ckeyRHvZLNvdhsj9sUhP6AFJzb8RCe7T-Ch9kp_h9wN9IxMHT-s096erWL8HQXHnew/exec"
#define GROUP      "1" 
#define DEVICE_ID  "ESP_MPSTME"

// ==========================================
// PIN DEFINITIONS
// ==========================================
#define MODEM_RST      5
#define MODEM_PWKEY    4
#define MODEM_POWER_ON 23
#define MODEM_TX       27
#define MODEM_RX       26

#define PMS_RX 32
#define PMS_TX 33
#define MS_RX  19
#define MS_TX  18
#define MHZ_RX 22
#define MHZ_TX 21

// ==========================================
// GLOBAL OBJECTS & VARIABLES
// ==========================================
SoftwareSerial pmsSerial(PMS_RX, PMS_TX);
Plantower_PMS7003 pms7003 = Plantower_PMS7003();
ErriezMHZ19B mhz19b(&Serial2);

TinyGsm modem(Serial1);
TinyGsmClientSecure gsmClient(modem); 

struct SensorData {
  String lux;
  String pressure;
  String temp;
  String humidity;
};

SensorData sensor_data;
char output[256];
unsigned long lastReport = 0;
const unsigned long REPORT_INTERVAL = 15000; 
int pm25 = 0;

// ==========================================
// HELPER FUNCTIONS
// ==========================================
void printTS() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo, 0)){ 
    unsigned long m = millis();
    unsigned long sec = (m / 1000) % 60;
    unsigned long min = (m / 60000) % 60;
    unsigned long hr = (m / 3600000);
    char ts[25];
    sprintf(ts, "[UPTIME %02lu:%02lu:%02lu] ", hr, min, sec);
    Serial.print(ts);
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
  if(!modem.isGprsConnected()) {
    printTS(); Serial.println(F("[Cellular] GPRS Disconnected. Reconnecting..."));
    modem.gprsConnect(apn, gprsUser, gprsPass);
  }

  if(modem.isGprsConnected()){
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
    
    printTS(); Serial.println(F("[Cloud] Sending via Cellular: To Google Sheet"));
    http.begin(gsmClient, url);
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

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  Serial.println(); 
  printTS(); Serial.println(F("--- Weather Station Initializing ---"));

  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);

  digitalWrite(MODEM_POWER_ON, HIGH);
  digitalWrite(MODEM_PWKEY, LOW);
  delay(1000);
  digitalWrite(MODEM_PWKEY, HIGH);

  Serial1.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000); 

  modem.restart();
  if (!modem.waitForNetwork(60000L)) {
    printTS(); Serial.println(F("[Cellular] Network failed."));
  } else {
    printTS(); Serial.println(F("[Cellular] Network connected!"));
  }

  modem.gprsConnect(apn, gprsUser, gprsPass);

  int year, month, day, hour, min, sec;
  float timezone;
  if (modem.getNetworkTime(&year, &month, &day, &hour, &min, &sec, &timezone)) {
    struct tm t = {0};
    t.tm_year = year - 1900; t.tm_mon = month - 1; t.tm_mday = day;
    t.tm_hour = hour; t.tm_min = min; t.tm_sec = sec;
    time_t timeSinceEpoch = mktime(&t);
    struct timeval tv = { .tv_sec = timeSinceEpoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);
  }

  pmsSerial.begin(9600);
  pms7003.init(&pmsSerial);
  Serial2.begin(9600, SERIAL_8N1, MHZ_RX, MHZ_TX);

  while (!mhz19b.detect()) { delay(2000); }
  while (mhz19b.isWarmingUp()) {
    printTS(); Serial.println(F("MH-Z19B Warming up..."));
    delay(5000); 
  }
  printTS(); Serial.println(F("--- All Sensors Ready! ---"));
}

// ==========================================
// LOOP
// ==========================================
void loop() {
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