//วัดอุณหภูมิความชื้น LCD / BLYNK
//แสดงวันที่ / เวลา LCD / BLYNK
//04-10-68 วัดพลังงานไฟฟ้า / LCD
//07-10-68 ทดสอบเตรียมสอบ

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <DHT.h>
#include <PZEM004Tv30.h>

//===== Wifi =====
char ssid[] = "LEA";
char pass[] = "11111111";
char auth[] = "3J1ZyG1Lh912BrfV2GTXhnYCGrt0oe_a";

//===== Objects =====
BlynkTimer TimerReadSensor;
BlynkTimer TimerClockDisplay;
BlynkTimer TimerReadAMP;
BlynkTimer TimerReadMQ2;
WidgetRTC rtc;

LiquidCrystal_I2C lcd(0x27, 20, 4);
PZEM004Tv30 pzem(D6, D5);

//===== DHT11 =====
#define DHTPIN 0
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

void ReadAMP() {
  float voltage = pzem.voltage();
  float current = pzem.current();
  float power = pzem.power();
  float energy = pzem.energy();

  if (isnan(voltage) || isnan(current) || isnan(power) || isnan(energy)) {
    Serial.println("Error reading from PZEM!");
    return;
  }

  Serial.printf("V: %.1fV | I: %.2fA | P: %.1fW | E: %.3fkWh\n", voltage, current, power, energy);

  lcd.setCursor(0, 2);
  lcd.printf("[A1] V:%dV I:%.2fA", (int)voltage, current);

  //ส่งค่าไป Blynk
  Blynk.virtualWrite(V20, (int)voltage);
  Blynk.virtualWrite(V21, current);
  Blynk.virtualWrite(V22, (int)power);
  Blynk.virtualWrite(V23, energy);

  if (power <= 0.0) {
    Blynk.virtualWrite(V11, 0);
    Serial.println(" Power = 0 > LED OFF");
  } else {
    // ถ้ามีกำลังไฟ → เปิด LED
    Blynk.virtualWrite(V11, 255);  // หรือใช้ค่า 1 ก็ได้ ถ้าเป็น LED widget
    Serial.println(" Power detected LED ON");
  }
}

void ReadMQ2() {
  int smokePin = A0;               // ขา A0 ต่อกับขา AO ของ MQ-2
  int val = analogRead(smokePin);  // อ่านค่า ADC (0–1023)

  Serial.print("ค่าควันที่อ่านได้: ");
  Serial.println(val);


  if (val > 400) {  // ถ้าค่าควันเกินเกณฑ์
    Serial.println(" ตรวจพบควัน!");
    lcd.setCursor(19, 0);
    lcd.print("*");  // แสดงสัญลักษณ์เตือนบน LCD


    Blynk.virtualWrite(V7, 0);

  } else {
    Serial.println("อากาศปกติ");
    lcd.setCursor(19, 0);
    lcd.print(" ");


    Blynk.virtualWrite(V7, 255);
  }
}

void ReadSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  lcd.setCursor(0, 1);  // แถว 1 (0 คือแถวบนสุด)
  lcd.printf("Temp:%d C Humi:%d %%  ", (int)t, (int)h);


  Blynk.virtualWrite(V6, t);  // Temp
  Blynk.virtualWrite(V5, h);  // Humidity

  Serial.printf("Temp: %.1f °C | Humidity: %.1f %%\n", t, h);
}

void ClockDisplay() {
  char Time[10], date[11];
  sprintf(date, "%02d/%02d/%04d", day(), month(), year() + 543);
  sprintf(Time, "%02d:%02d:%02d", hour(), minute(), second());

  lcd.setCursor(0, 0);
  lcd.printf("%s      AUTO", Time);
  Blynk.virtualWrite(V49, String(Time));
  Blynk.virtualWrite(V50, String(date));
  Serial.printf("Time: %s | Date: %s\n", Time, date);
}

//===== Blynk RTC =====
BLYNK_CONNECTED() {
  rtc.begin();
}

//===== Setup =====
void setup() {
  Serial.begin(115200);
  dht.begin();

  Wire.begin(D2, D1);  // กำหนด SDA/SCL
  lcd.begin();
  lcd.backlight();

  Blynk.begin(auth, ssid, pass, "blynk.en-26.com", 9600);
  Serial.println("Connecting Blynk : ");
  setSyncInterval(10 * 60);

  TimerReadSensor.setInterval(1301L, ReadSensor);  // ไม่ชน 900
  TimerReadAMP.setInterval(1709L, ReadAMP);        // ไม่ชน 900 หรือ 1300
  TimerClockDisplay.setInterval(900L, ClockDisplay);
  TimerReadMQ2.setInterval(1999L, ReadMQ2);

  ReadSensor();
  ReadAMP();
  ClockDisplay();
  ReadMQ2();
}

void loop() {
  Blynk.run();
  TimerReadSensor.run();
  TimerClockDisplay.run();
  TimerReadAMP.run();
  TimerReadMQ2.run();
}
