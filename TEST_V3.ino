#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <DHT.h>
#include <PZEM004Tv30.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Keypad_I2C.h>
#include <HTTPClient.h>
#include <EEPROM.h>

char ssid[] = "Rice_Project";
char pass[] = "12345678";
char auth[] = "3J1ZyG1Lh912BrfV2GTXhnYCGrt0oe_a";

#define BOT_TOKEN "8212167335:AAFWiJjKnin9FSqSVmbx5pbooh3pBwZyJOI"
#define CHAT_ID "-5096576497"

#define GSHEET_URL "https://script.google.com/macros/s/AKfycbwnPdo6qeg8VAPYRoNtzNKSG2RevbACrZXr3_2fi1NGSLRoWpm2QsOzhTpECh_SeI8h/exec"

WiFiClientSecure tgClient;
UniversalTelegramBot bot(BOT_TOKEN, tgClient);

BlynkTimer TimerElectricPower;
BlynkTimer TimerReadSensor;
BlynkTimer TimerClockDisplay;
BlynkTimer TimerReadAMP;
BlynkTimer TimerReadMQ2;
BlynkTimer TimerTelegram;
BlynkTimer TimerTempSheet;
WidgetRTC rtc;

LiquidCrystal_I2C lcd(0x27, 20, 4);

#define EEPROM_SIZE 128
#define ADDR_tempLow 0
#define ADDR_tempHigh 4
#define ADDR_humiLow 8
#define ADDR_humiHigh 12

#define ADDR_A1_Vlow 16
#define ADDR_A1_Vhigh 20
#define ADDR_A1_Clow 24
#define ADDR_A1_Chigh 28

#define ADDR_A2_Vlow 32
#define ADDR_A2_Vhigh 36
#define ADDR_A2_Clow 40
#define ADDR_A2_Chigh 44

#define ADDR_smokeTh 48
#define ADDR_A1_RunTh 52
#define ADDR_A2_RunTh 56

#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
#define KEYPAD_ADDR 0x20
#define FLAME_A0 34
#define MQ2_PIN 35
#define POWER_DETECT_PIN 32


HardwareSerial PZSerial(1);
PZEM004Tv30 pzem1(&PZSerial, 0x01);
PZEM004Tv30 pzem2(&PZSerial, 0x02);

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[ROWS] = { 0, 1, 2, 3 };
byte colPins[COLS] = { 4, 5, 6, 7 };
Keypad_I2C Kpd(makeKeymap(keys), rowPins, colPins, ROWS, COLS, KEYPAD_ADDR);
int lcdPage = 1;

// FLAGS
bool powerLost = false;
bool tgTempAlert = false;
bool tgTempSent = false;
bool tgHumiAlert = false;
bool tgHumiSent = false;
bool tgSmokeAlert = false;
bool tgSmokeSent = false;
bool tgFlameSent = false;
bool tgAC1OnSent = false;
bool tgAC1OffSent = false;
bool tgAC2OnSent = false;
bool tgAC2OffSent = false;
bool tgPowerOffSent = false;
bool tgPowerOnSent = false;
bool tgA1VoltSent = false;
bool tgA1CurrSent = false;
bool tgA2VoltSent = false;
bool tgA2CurrSent = false;
bool sheetFlameSent = false;
bool sheetSmokeSent = false;
// เปิด-ปิดการแจ้งเตือน
bool enableTempAlert = true;
bool enableHumiAlert = true;
bool enableSmokeAlert = true;
bool enableFlameAlert = true;
bool enablePowerAlert = true;
bool enableAC1Alert = true;
bool enableAC2Alert = true;
bool tgFlameAlert = false;
float lastTemp = 0;
float lastHumi = 0;

float setA1 = 3.0;
float setA2 = 3.0;

bool lastAC1 = false;
bool lastCompA1 = false;

bool lastAC2 = false;
bool lastCompA2 = false;



//USER TEMP / HUMI SET
float tempLow = 20;
float tempHigh = 32;
float humiLow = 40;
float humiHigh = 55;
//ELECTRIC LIMIT
float A1_Vlow = 200;
float A1_Vhigh = 245;
float A1_Clow = 2;
float A1_Chigh = 16;
float A2_Vlow = 200;
float A2_Vhigh = 245;
float A2_Clow = 2;
float A2_Chigh = 16;

int smokeThreshold = 1500;
int flameThreshold = 1500;

unsigned long a1CurrStart = 0;
bool a1CurrTriggering = false;
int a1CurrState = 0;

unsigned long a2CurrStart = 0;
bool a2CurrTriggering = false;
int a2CurrState = 0;

bool lcdNeedsUpdate = false;
unsigned long lastLCDUpdate = 0;
const unsigned long LCD_UPDATE_INTERVAL = 900;
bool lcdPageChanged = false;

BLYNK_WRITE(V31) {
  tempLow = param.asFloat();
  EEPROM.put(ADDR_tempLow, tempLow);
  EEPROM.commit();
}
BLYNK_WRITE(V32) {
  tempHigh = param.asFloat();
  EEPROM.put(ADDR_tempHigh, tempHigh);
  EEPROM.commit();
}

BLYNK_WRITE(V33) {
  humiLow = param.asFloat();
  EEPROM.put(ADDR_humiLow, humiLow);
  EEPROM.commit();
}

BLYNK_WRITE(V34) {
  humiHigh = param.asFloat();
  EEPROM.put(ADDR_humiHigh, humiHigh);
  EEPROM.commit();
}

BLYNK_WRITE(V35) {
  A1_Vlow = param.asFloat();
  EEPROM.put(ADDR_A1_Vlow, A1_Vlow);
  EEPROM.commit();
}

BLYNK_WRITE(V36) {
  A1_Vhigh = param.asFloat();
  EEPROM.put(ADDR_A1_Vhigh, A1_Vhigh);
  EEPROM.commit();
}

BLYNK_WRITE(V37) {
  A1_Clow = param.asFloat();
  EEPROM.put(ADDR_A1_Clow, A1_Clow);
  EEPROM.commit();
}

BLYNK_WRITE(V38) {
  A1_Chigh = param.asFloat();
  EEPROM.put(ADDR_A1_Chigh, A1_Chigh);
  EEPROM.commit();
}

BLYNK_WRITE(V39) {
  A2_Vlow = param.asFloat();
  EEPROM.put(ADDR_A2_Vlow, A2_Vlow);
  EEPROM.commit();
}

BLYNK_WRITE(V40) {
  A2_Vhigh = param.asFloat();
  EEPROM.put(ADDR_A2_Vhigh, A2_Vhigh);
  EEPROM.commit();
}

BLYNK_WRITE(V41) {
  A2_Clow = param.asFloat();
  EEPROM.put(ADDR_A2_Clow, A2_Clow);
  EEPROM.commit();
}

BLYNK_WRITE(V42) {
  A2_Chigh = param.asFloat();
  EEPROM.put(ADDR_A2_Chigh, A2_Chigh);
  EEPROM.commit();
}

// เปิด-ปิด การแจ้งเตือน
BLYNK_WRITE(V51) {
  enableTempAlert = param.asInt();
}
BLYNK_WRITE(V52) {
  enableHumiAlert = param.asInt();
}
BLYNK_WRITE(V53) {
  enableSmokeAlert = param.asInt();
}
BLYNK_WRITE(V54) {
  enableFlameAlert = param.asInt();
}
BLYNK_WRITE(V55) {
  enablePowerAlert = param.asInt();
}
BLYNK_WRITE(V56) {
  enableAC1Alert = param.asInt();
}
BLYNK_WRITE(V57) {
  enableAC2Alert = param.asInt();
}
BLYNK_WRITE(V58) {
  smokeThreshold = param.asInt();
  EEPROM.put(ADDR_smokeTh, smokeThreshold);
  EEPROM.commit();
}
BLYNK_WRITE(V45) {
  setA1 = param.asFloat();
  EEPROM.put(ADDR_A1_RunTh, setA1);
  EEPROM.commit();
}

BLYNK_WRITE(V46) {
  setA2 = param.asFloat();
  EEPROM.put(ADDR_A2_RunTh, setA2);
  EEPROM.commit();
}



//จัดการ LCD โดยไม่ให้กระพริบ
void UpdateLCD_NoBlink() {
  static unsigned long lastUpdate = 0;
  unsigned long currentMillis = millis();

  if (lcdPageChanged || (currentMillis - lastUpdate >= LCD_UPDATE_INTERVAL)) {
    if (lcdPageChanged) {
      for (int row = 1; row < 4; row++) {
        lcd.setCursor(0, row);
        lcd.print("                    ");
      }
      lcdPageChanged = false;
    }
    if (lcdPage == 1) {
      DisplayLCD_Page1();
    } else {
      DisplayLCD_Page2();
    }
    lastUpdate = currentMillis;
  }
}

void DisplayLCD_Page1() {

  static float prevTemp = -1;
  static float prevHumi = -1;
  static float prevV1 = -1, prevC1 = -1;
  static float prevV2 = -1, prevC2 = -1;

  float v1 = pzem1.voltage();
  float c1 = pzem1.current();
  float v2 = pzem2.voltage();
  float c2 = pzem2.current();
  bool needUpdate = false;

  if (prevTemp != lastTemp || prevHumi != lastHumi) {
    lcd.setCursor(0, 1);
    lcd.print("Temp:   C Humi:   %  ");
    lcd.setCursor(5, 1);
    lcd.print((int)lastTemp);
    lcd.setCursor(15, 1);
    lcd.print((int)lastHumi);
    prevTemp = lastTemp;
    prevHumi = lastHumi;
    needUpdate = true;
  }

  if (prevV1 != v1 || prevC1 != c1) {
    lcd.setCursor(0, 2);
    lcd.print("[A1]:     V      A ");
    if (!isnan(v1) && !isnan(c1)) {
      lcd.setCursor(6, 2);
      lcd.print((int)v1);
      lcd.setCursor(13, 2);
      if (c1 < 12) lcd.print(" ");
      lcd.print(c1, 1);
    } else {
      lcd.setCursor(6, 2);
      lcd.print("----");
      lcd.setCursor(10, 2);
      lcd.print("----");
    }
    prevV1 = v1;
    prevC1 = c1;
    needUpdate = true;
  }

  if (prevV2 != v2 || prevC2 != c2) {
    lcd.setCursor(0, 3);
    lcd.print("[A2]:     V      A ");
    if (!isnan(v2) && !isnan(c2)) {
      lcd.setCursor(6, 3);
      lcd.print((int)v2);
      lcd.setCursor(13, 3);
      if (c2 < 12) lcd.print(" ");
      lcd.print(c2, 1);
    } else {
      lcd.setCursor(6, 3);
      lcd.print("----");
      lcd.setCursor(10, 3);
      lcd.print("----");
    }
    prevV2 = v2;
    prevC2 = c2;
    needUpdate = true;
  }
}

void DisplayLCD_Page2() {

  lcd.setCursor(0, 1);
  lcd.print("Smoke: ");
  lcd.print(tgSmokeAlert ? "DETECT " : "Normal ");
  lcd.print(" ");

  lcd.setCursor(0, 2);
  lcd.print("Flame: ");
  lcd.print(tgFlameAlert ? "DETECT " : "Normal ");
  lcd.print(" ");

  lcd.setCursor(0, 3);
  lcd.print("Power: ");
  lcd.print(powerLost ? "OFF " : "ON ");
  lcd.print(" ");
}

void ReadAMP() {

  float v1 = pzem1.voltage();
  float c1 = pzem1.current();
  float p1 = pzem1.power();
  float e1 = pzem1.energy();
  if (!isnan(v1)) {
    Blynk.virtualWrite(V20, v1);
    Blynk.virtualWrite(V21, c1);
    Blynk.virtualWrite(V22, p1);
    Blynk.virtualWrite(V23, e1);
    if (!isnan(p1) && p1 > 50) Blynk.virtualWrite(V11, 255);
    else Blynk.virtualWrite(V11, 0);
  }
  float v2 = pzem2.voltage();
  float c2 = pzem2.current();
  float p2 = pzem2.power();
  float e2 = pzem2.energy();
  if (!isnan(v2)) {
    Blynk.virtualWrite(V24, v2);
    Blynk.virtualWrite(V25, c2);
    Blynk.virtualWrite(V26, p2);
    Blynk.virtualWrite(V27, e2);
    if (!isnan(p2) && p2 > 50) Blynk.virtualWrite(V12, 255);
    else Blynk.virtualWrite(V12, 0);
  }
}

void ReadMQ2() {

  int smokeVal = analogRead(MQ2_PIN);
  int flameVal = analogRead(FLAME_A0);

  smokeVal = constrain(smokeVal, 0, 4095);
  flameVal = constrain(flameVal, 0, 4095);

  // ------------------- SMOKE -------------------
  static unsigned long smokeStartTime = 0;
  static bool smokeConfirmed = false;

  if (smokeVal > smokeThreshold) {

    if (smokeStartTime == 0) {
      smokeStartTime = millis();
    }

    if (millis() - smokeStartTime >= 3000) {
      tgSmokeAlert = true;

      if (!smokeConfirmed) {
        smokeConfirmed = true;
        SendSmokeToSheet(smokeVal);
      }
    }

  } else {
    smokeStartTime = 0;
    tgSmokeAlert = false;
    smokeConfirmed = false;
    tgSmokeSent = false;
  }

  // ------------------- FLAME (แก้ไขใหม่) -------------------
  static unsigned long flameStartTime = 0;
  static bool flameConfirmed = false;

  if (flameVal < flameThreshold) {

    if (flameStartTime == 0) {
      flameStartTime = millis();
    }

    // ต้องตรวจพบต่อเนื่อง 2 วินาที
    if (millis() - flameStartTime >= 2000) {
      tgFlameAlert = true;

      if (!flameConfirmed) {
        flameConfirmed = true;

        if (!sheetFlameSent) {
          SendFlameToSheet(flameVal);
          sheetFlameSent = true;
        }
      }
    }

  } else {

    flameStartTime = 0;

    // รีเซ็ตเมื่อไฟหายจริง
    if (flameConfirmed) {
      flameConfirmed = false;
      tgFlameAlert = false;
      tgFlameSent = false;
      sheetFlameSent = false;
    }
  }

  // ------------------- BLYNK -------------------
  Blynk.virtualWrite(V7, smokeVal);
  Blynk.virtualWrite(V8, flameVal);
  Blynk.virtualWrite(V30, smokeVal);

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();

    Serial.print("Smoke: ");
    Serial.print(smokeVal);
    Serial.print(" | Flame: ");
    Serial.println(flameVal);
  }
}

void ReadSensor() {

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) return;
  lastTemp = t;
  lastHumi = h;
  Blynk.virtualWrite(V6, t);
  Blynk.virtualWrite(V5, h);
  tgTempAlert = (t > tempHigh || t < tempLow);
  if (!tgTempAlert) tgTempSent = false;
  tgHumiAlert = (h > humiHigh || h < humiLow);
  if (!tgHumiAlert) tgHumiSent = false;
}

void ClockDisplay() {

  static char lastTime[10] = "";
  static char lastDate[11] = "";

  char Time[10], date[11];
  sprintf(Time, "%02d:%02d:%02d", hour(), minute(), second());
  sprintf(date, "%02d/%02d/%04d", day(), month(), year() + 543);

  if (strcmp(Time, lastTime) != 0) {

    strcpy(lastTime, Time);
    lcd.setCursor(0, 0);
    lcd.print(Time);

    for (int i = strlen(Time); i < 20; i++) {
      lcd.print(" ");
    }
  }
  Blynk.virtualWrite(V49, Time);
  Blynk.virtualWrite(V50, date);
}


void TimerCheckElectricPower() {

  static int lastState = HIGH;
  int currentState = digitalRead(POWER_DETECT_PIN);

  if (currentState == lastState) return;
  if (currentState == HIGH) {
    powerLost = false;
    Blynk.virtualWrite(V9, 255);
    SendPowerToSheet("ON");

    if (enablePowerAlert && !tgPowerOnSent) {
      bot.sendMessage(CHAT_ID, "✅ ไฟฟ้ากลับมาแล้ว", "");
      tgPowerOnSent = true;
      tgPowerOffSent = false;
    }
  } else {
    powerLost = true;
    Blynk.virtualWrite(V9, 0);
    SendPowerToSheet("OFF");

    if (enablePowerAlert && !tgPowerOffSent) {
      bot.sendMessage(CHAT_ID, "⚠️ ไฟฟ้าดับ!", "");
      tgPowerOffSent = true;
      tgPowerOnSent = false;
    }
  }

  lastState = currentState;
}


void SendTelegramTask() {

  static char msg[200];
  if (enableTempAlert && tgTempAlert) {
    if (!tgTempSent) {
      snprintf(msg, sizeof(msg),
               "🌡 อุณหภูมิผิดปกติ\n"
               "อุณหภูมิปัจจุบัน %.1f°C\n"
               "ช่วงที่กำหนด %.1f - %.1f °C",
               lastTemp, tempLow, tempHigh);
      bot.sendMessage(CHAT_ID, msg, "");
      tgTempSent = true;
      return;
    }
  } else {
    tgTempSent = false;
  }
  if (enableHumiAlert && tgHumiAlert) {
    if (!tgHumiSent) {
      snprintf(msg, sizeof(msg),
               "💧 ความชื้นผิดปกติ\n"
               "ความชื้นปัจจุบัน %.1f%%\n"
               "ช่วงที่กำหนด %.1f - %.1f %%",
               lastHumi, humiLow, humiHigh);
      bot.sendMessage(CHAT_ID, msg, "");
      tgHumiSent = true;
      return;
    }
  } else {
    tgHumiSent = false;
  }

  if (enableSmokeAlert && tgSmokeAlert) {
    if (!tgSmokeSent) {
      bot.sendMessage(CHAT_ID, "💨 ตรวจพบควัน", "");
      tgSmokeSent = true;
      return;
    }
  } else {
    tgSmokeSent = false;
  }

  static unsigned long lastFlameMsg = 0;
  const unsigned long flameCooldown = 60000;  // 60 วิ


  if (enableFlameAlert) {

    if (tgFlameAlert) {

      if (!tgFlameSent) {
        bot.sendMessage(CHAT_ID, "🔥 ตรวจพบเปลวไฟ!", "");
        tgFlameSent = true;
        return;
      }

    } else {
      // รีเซ็ตเฉพาะเมื่อไฟหายจริง
      tgFlameSent = false;
    }
  }

  float p1 = pzem1.power();
  float p2 = pzem2.power();

  if (enableAC1Alert && !isnan(p1)) {
    if (p1 > 40) {
      if (!tgAC1OnSent) {
        bot.sendMessage(CHAT_ID, "❄️ เครื่องปรับอากาศ A1 กำลังทำงาน", "");
        tgAC1OnSent = true;
        tgAC1OffSent = false;
        return;
      }
    } else {
      if (!tgAC1OffSent) {
        bot.sendMessage(CHAT_ID, "⚠️ เครื่องปรับอากาศ A1 หยุดทำงาน", "");
        tgAC1OffSent = true;
        tgAC1OnSent = false;
        return;
      }
    }
  }

  if (enableAC2Alert && !isnan(p2)) {
    if (p2 > 40) {
      if (!tgAC2OnSent) {
        bot.sendMessage(CHAT_ID, "❄️ เครื่องปรับอากาศ A2 กำลังทำงาน", "");
        tgAC2OnSent = true;
        tgAC2OffSent = false;
        return;
      }
    } else {
      if (!tgAC2OffSent) {
        bot.sendMessage(CHAT_ID, "⚠️ เครื่องปรับอากาศ A2 หยุดทำงาน", "");
        tgAC2OffSent = true;
        tgAC2OnSent = false;
        return;
      }
    }
  }

  float v1 = pzem1.voltage();
  float c1 = pzem1.current();
  float v2 = pzem2.voltage();
  float c2 = pzem2.current();

  // ===== A1 Current Alert =====
  if (enableAC1Alert && !isnan(c1)) {

    int currentState = 0;
    if (c1 > A1_Chigh) currentState = 1;       // OVERLOAD
    else if (c1 < A1_Clow) currentState = -1;  // UNDERLOAD

    if (currentState != 0) {

      if (!a1CurrTriggering) {
        a1CurrTriggering = true;
        a1CurrStart = millis();
      }

      if (millis() - a1CurrStart >= 3000 && currentState != a1CurrState) {

        if (currentState == 1) {
          snprintf(msg, sizeof(msg),
                   "⚠️ A1 OVERLOAD\nกระแส %.2f A\nเกิน %.2f A",
                   c1, A1_Chigh);
        } else {
          snprintf(msg, sizeof(msg),
                   "⚠️ A1 UNDERLOAD\nกระแส %.2f A\nต่ำกว่า %.2f A",
                   c1, A1_Clow);
        }

        bot.sendMessage(CHAT_ID, msg, "");
        a1CurrState = currentState;
        return;
      }

    } else {

      if (a1CurrState != 0) {
        bot.sendMessage(CHAT_ID,
                        "✅ A1 กระแสกลับสู่ปกติแล้ว",
                        "");
      }

      a1CurrState = 0;
      a1CurrTriggering = false;
    }
  }


  // ===== A2 Current Alert =====
  if (enableAC2Alert && !isnan(c2)) {

    int currentState = 0;
    if (c2 > A2_Chigh) currentState = 1;       // OVERLOAD
    else if (c2 < A2_Clow) currentState = -1;  // UNDERLOAD

    if (currentState != 0) {

      if (!a2CurrTriggering) {
        a2CurrTriggering = true;
        a2CurrStart = millis();
      }

      if (millis() - a2CurrStart >= 3000 && currentState != a2CurrState) {

        if (currentState == 1) {
          snprintf(msg, sizeof(msg),
                   "⚠️ A2 OVERLOAD\nกระแส %.2f A\nเกิน %.2f A",
                   c2, A2_Chigh);
        } else {
          snprintf(msg, sizeof(msg),
                   "⚠️ A2 UNDERLOAD\nกระแส %.2f A\nต่ำกว่า %.2f A",
                   c2, A2_Clow);
        }

        bot.sendMessage(CHAT_ID, msg, "");
        a2CurrState = currentState;
        return;
      }

    } else {

      if (a2CurrState != 0) {
        bot.sendMessage(CHAT_ID,
                        "✅ A2 กระแสกลับสู่ปกติแล้ว",
                        "");
      }

      a2CurrState = 0;
      a2CurrTriggering = false;
    }
  }
}



void SendTempToSheet() {

  if (WiFi.status() != WL_CONNECTED) return;

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.begin(client, GSHEET_URL);
  http.addHeader("Content-Type", "application/json");

  String payload =
    "{"
    "\"type\":\"temp_humi\","
    "\"temp\":"
    + String(t) + ","
                  "\"humi\":"
    + String(h) + "}";

  http.POST(payload);
  http.end();
}

void SendSmokeToSheet(int smokeVal) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.begin(client, GSHEET_URL);
  http.addHeader("Content-Type", "application/json");

  String payload =
    "{"
    "\"type\":\"smoke_gas\","
    "\"smoke\":"
    + String(smokeVal) + ","
                         "\"status\":\"DETECT\""
                         "}";

  http.POST(payload);
  http.end();
}

void SendFlameToSheet(int flameVal) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.begin(client, GSHEET_URL);
  http.addHeader("Content-Type", "application/json");

  String payload =
    "{"
    "\"type\":\"fire_flame\","
    "\"flame\":"
    + String(flameVal) + ","
                         "\"status\":\"DETECT\""
                         "}";

  http.POST(payload);
  http.end();
}

void SendPowerToSheet(String status) {

  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.begin(client, GSHEET_URL);
  http.addHeader("Content-Type", "application/json");

  String payload =
    "{"
    "\"type\":\"power_outage\","
    "\"status\":\""
    + status + "\""
               "}";

  http.POST(payload);
  http.end();
}

void SendAC1_EventToSheet(float powerValue, bool acNow, bool compNow) {

  if (WiFi.status() != WL_CONNECTED) return;

  float v = pzem1.voltage();
  float c = pzem1.current();
  float e = pzem1.energy();

  String acStatus = acNow ? "ON" : "OFF";
  String compStatus = compNow ? "ON" : "OFF";

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.begin(client, GSHEET_URL);
  http.addHeader("Content-Type", "application/json");

  String payload =
    "{"
    "\"type\":\"ac1_log\","
    "\"voltage\":"
    + String(v, 1) + ","
                     "\"current\":"
    + String(c, 2) + ","
                     "\"power\":"
    + String(powerValue, 0) + ","
                              "\"energy\":"
    + String(e, 2) + ","
                     "\"ac_status\":\""
    + acStatus + "\","
                 "\"comp_status\":\""
    + compStatus + "\""
                   "}";

  http.POST(payload);
  http.end();
}


void SendAC2_EventToSheet(float powerValue, bool acNow, bool compNow) {

  if (WiFi.status() != WL_CONNECTED) return;

  float v = pzem2.voltage();
  float c = pzem2.current();
  float e = pzem2.energy();

  String acStatus = acNow ? "ON" : "OFF";
  String compStatus = compNow ? "ON" : "OFF";

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.begin(client, GSHEET_URL);
  http.addHeader("Content-Type", "application/json");

  String payload =
    "{"
    "\"type\":\"ac2_log\","
    "\"voltage\":"
    + String(v, 1) + ","
                     "\"current\":"
    + String(c, 2) + ","
                     "\"power\":"
    + String(powerValue, 0) + ","
                              "\"energy\":"
    + String(e, 2) + ","
                     "\"ac_status\":\""
    + acStatus + "\","
                 "\"comp_status\":\""
    + compStatus + "\""
                   "}";

  http.POST(payload);
  http.end();
}


void CheckAC_EventChange() {

  float c1 = pzem1.current();
  float p1 = pzem1.power();

  float c2 = pzem2.current();
  float p2 = pzem2.power();

  // ================== AC1 ==================
  if (!isnan(c1) && !isnan(p1)) {

    bool ac1Now = (c1 > 0.2);
    bool comp1Now = (c1 > setA1);

    if (ac1Now != lastAC1 || comp1Now != lastCompA1) {
      SendAC1_EventToSheet(p1, ac1Now, comp1Now);  // 👈 ส่ง p1 ไม่ใช่ c1
    }

    lastAC1 = ac1Now;
    lastCompA1 = comp1Now;
  }

  // ================== AC2 ==================
  if (!isnan(c2) && !isnan(p2)) {

    bool ac2Now = (c2 > 0.2);
    bool comp2Now = (c2 > setA2);

    if (ac2Now != lastAC2 || comp2Now != lastCompA2) {
      SendAC2_EventToSheet(p2, ac2Now, comp2Now);  // 👈 ส่ง p2
    }

    lastAC2 = ac2Now;
    lastCompA2 = comp2Now;
  }
}




BLYNK_CONNECTED() {
  rtc.begin();
  Blynk.virtualWrite(V31, tempLow);
  Blynk.virtualWrite(V32, tempHigh);
  Blynk.virtualWrite(V33, humiLow);
  Blynk.virtualWrite(V34, humiHigh);
  Blynk.virtualWrite(V58, smokeThreshold);
  Blynk.virtualWrite(V35, A1_Vlow);
  Blynk.virtualWrite(V36, A1_Vhigh);
  Blynk.virtualWrite(V37, A1_Clow);
  Blynk.virtualWrite(V38, A1_Chigh);

  Blynk.virtualWrite(V39, A2_Vlow);
  Blynk.virtualWrite(V40, A2_Vhigh);
  Blynk.virtualWrite(V41, A2_Clow);
  Blynk.virtualWrite(V42, A2_Chigh);
}

void setup() {

  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(ADDR_tempLow, tempLow);
  EEPROM.get(ADDR_tempHigh, tempHigh);
  EEPROM.get(ADDR_humiLow, humiLow);
  EEPROM.get(ADDR_humiHigh, humiHigh);

  EEPROM.get(ADDR_A1_Vlow, A1_Vlow);
  EEPROM.get(ADDR_A1_Vhigh, A1_Vhigh);
  EEPROM.get(ADDR_A1_Clow, A1_Clow);
  EEPROM.get(ADDR_A1_Chigh, A1_Chigh);

  EEPROM.get(ADDR_A2_Vlow, A2_Vlow);
  EEPROM.get(ADDR_A2_Vhigh, A2_Vhigh);
  EEPROM.get(ADDR_A2_Clow, A2_Clow);
  EEPROM.get(ADDR_A2_Chigh, A2_Chigh);
  EEPROM.get(ADDR_A1_RunTh, setA1);
  EEPROM.get(ADDR_A2_RunTh, setA2);

  EEPROM.get(ADDR_smokeTh, smokeThreshold);
  



  pinMode(POWER_DETECT_PIN, INPUT);
  pinMode(FLAME_A0, INPUT);
  dht.begin();
  Wire.begin(21, 22);
  lcd.begin();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");
  PZSerial.begin(9600, SERIAL_8N1, 16, 17);
  Blynk.begin(auth, ssid, pass, "blynk.en-26.com", 9600);
  bot.sendMessage(CHAT_ID, "✅ ระบบทำงานแล้ว", "");

  tgClient.setInsecure();
  TimerReadMQ2.setInterval(487, ReadMQ2);            // เร็ว
  TimerClockDisplay.setInterval(900, ClockDisplay);  // ~1 วิ
  TimerElectricPower.setInterval(1127, TimerCheckElectricPower);

  TimerReadAMP.setInterval(1741, ReadAMP);
  TimerReadSensor.setInterval(2671, ReadSensor);

  TimerTelegram.setInterval(5237, SendTelegramTask);
  TimerTempSheet.setInterval(901027, SendTempToSheet);
  Kpd.begin();

  for (int row = 1; row < 4; row++) {
    lcd.setCursor(0, row);
    lcd.print("                    ");
  }
  lastLCDUpdate = millis();
}

void loop() {

  Blynk.run();
  TimerReadSensor.run();
  TimerReadAMP.run();
  TimerClockDisplay.run();
  TimerReadMQ2.run();
  TimerTelegram.run();
  TimerElectricPower.run();
  TimerTempSheet.run();
  UpdateLCD_NoBlink();
  CheckAC_EventChange();

  char key = Kpd.getKey();
  if (key == '1') {
    lcdPage = 1;
    lcdPageChanged = true;
  }
  if (key == '2') {
    lcdPage = 2;
    lcdPageChanged = true;
  }
}