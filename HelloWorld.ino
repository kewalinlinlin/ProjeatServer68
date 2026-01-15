//LDC แสดง วัน/เวลา อุณหภูมิ/ชื้น
//ไม่พบภาษาแปลก 

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <WidgetRTC.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"

char auth[] = "3J1ZyG1Lh912BrfV2GTXhnYCGrt0oe_a";
char ssid[] = "LEA";
char pass[] = "11111111";

LiquidCrystal_I2C lcd(0x27, 20, 4);

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define LED_PIN 2

WidgetRTC rtc;
BlynkTimer TimerReadSensor;
BlynkTimer TimerShowClock;

char TimeStr[9];
char DateStr[11];

//เวลา NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600; // GMT+7
const int daylightOffset_sec = 0;

void ReadSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("อ่านค่า DHT11 ไม่สำเร็จ!");
    digitalWrite(LED_PIN, LOW);
    Blynk.virtualWrite(V7, 0);  
    Blynk.virtualWrite(V49, "");  
    Blynk.virtualWrite(V50, "");  
    return;
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("ไม่สามารถดึงเวลาได้");
    return;
  }

  strftime(TimeStr, sizeof(TimeStr), "%H:%M:%S", &timeinfo);
  strftime(DateStr, sizeof(DateStr), "%d/%m/%Y", &timeinfo);

  Blynk.virtualWrite(V5, h); 
  Blynk.virtualWrite(V6, t);     
  Blynk.virtualWrite(V49, TimeStr); 
  Blynk.virtualWrite(V50, DateStr); 
  Blynk.virtualWrite(V7, 255);    

  digitalWrite(LED_PIN, HIGH);

  Serial.printf("Temp: %.1f °C | Humidity: %.1f %% | Time: %s | Date: %s\n", t, h, TimeStr, DateStr);

  lcd.setCursor(0, 0); 
  lcd.printf("%s %s", DateStr, TimeStr);

  lcd.setCursor(0, 1); 
  lcd.printf("Temp: %.1f C   ", t);

  lcd.setCursor(0, 2); 
  lcd.printf("Humi: %.1f %%  ", h);
}

void ShowClock() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  strftime(TimeStr, sizeof(TimeStr), "%H:%M:%S", &timeinfo);
  strftime(DateStr, sizeof(DateStr), "%d/%m/%Y", &timeinfo);

  lcd.setCursor(0, 0);
  lcd.printf("%s %s", DateStr, TimeStr);

  Blynk.virtualWrite(V49, TimeStr);
  Blynk.virtualWrite(V50, DateStr);
}

BLYNK_CONNECTED() {
  rtc.begin();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  Wire.begin(D2, D1);


  lcd.init();
  lcd.backlight();
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(2000);

  Blynk.begin(auth, ssid, pass,"blynk.en-26.com", 9600);

  setSyncInterval(10 * 60);
  TimerReadSensor.setInterval(1500L, ReadSensor);
  TimerShowClock.setInterval(900L, ShowClock); // อัปเดตเวลา

  ReadSensor();
}

void loop() {
  Blynk.run();
  TimerReadSensor.run();
  TimerShowClock.run();
}