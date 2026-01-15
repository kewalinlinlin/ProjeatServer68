// แสดงวันที่ / เวลา LCD / BLYNK
// เพิ่ม Telegram Alert (แบบเสถียร ไม่รบกวนระบบเดิม)

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


char ssid[] = "LEA";
char pass[] = "11111111";
char auth[] = "3J1ZyG1Lh912BrfV2GTXhnYCGrt0oe_a";

unsigned long keypadOverrideTime = 0;
#define KEYPAD_OVERRIDE_TIMEOUT 10000  // 10 วินาที

#define BOT_TOKEN "8212167335:AAFWiJjKnin9FSqSVmbx5pbooh3pBwZyJOI"
#define CHAT_ID "-5096576497"

WiFiClientSecure tgClient;
UniversalTelegramBot bot(BOT_TOKEN, tgClient);

BlynkTimer timerElectricPower;
BlynkTimer TimerReadSensor;
BlynkTimer TimerClockDisplay;
BlynkTimer TimerReadAMP;
BlynkTimer TimerReadMQ2;
BlynkTimer TimerTelegram;
WidgetRTC rtc;

LiquidCrystal_I2C lcd(0x27, 20, 4);

#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
#define FLAME_A0 34
#define MQ2_PIN 35
#define POWER_DETECT_PIN 18
#define I2C_KEYPAD_ADDR 0x20

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

// PCF8574 P0–P3 = Row, P4–P7 = Column
byte rowPins[ROWS] = { 0, 1, 2, 3 };
byte colPins[COLS] = { 4, 5, 6, 7 };

Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, I2C_KEYPAD_ADDR);

bool keypadOverride = false;
bool powerLost = false;
bool tgTempAlert = false;
bool tgTempSent = false;
bool tgHumiAlert = false;
bool tgHumiSent = false;
bool tgSmokeAlert = false;
bool tgSmokeSent = false;
bool tgFlameAlert = false;
bool tgFlameSent = false;
bool tgAC1OnSent = false;
bool tgAC1OffSent = false;
bool tgAC2OnSent = false;
bool tgAC2OffSent = false;
bool tgPowerOffSent = false;
bool tgPowerOnSent = false;
float lastTemp = 0;
float lastHumi = 0;

// --- โหมด AUTO / MANUAL ---
enum SystemMode { MODE_AUTO,
                  MODE_MANUAL };
SystemMode currentMode = MODE_AUTO;  // เริ่มต้น AUTO

// --------------------- BLYNK ปุ่มสลับโหมด ---------------------
BLYNK_WRITE(V100) {
  if (keypadOverride) return;

  if (param.asInt() == 1) {
    currentMode = MODE_AUTO;
    Blynk.virtualWrite(V101, 0);
    updateModeLED();
  }
}

BLYNK_WRITE(V101) {
  if (keypadOverride) return;

  if (param.asInt() == 1) {
    currentMode = MODE_MANUAL;
    Blynk.virtualWrite(V100, 0);
    updateModeLED();
  }
}




// --------------------- ฟังก์ชันอ่าน PZEM ---------------------
void ReadAMP() {
  if (currentMode == MODE_MANUAL) return;  // MANUAL ไม่อ่านพลังงาน

  float v1 = pzem1.voltage();
  float c1 = pzem1.current();
  float p1 = pzem1.power();
  float e1 = pzem1.energy();

  if (!isnan(v1)) {
    lcd.setCursor(0, 2);
    lcd.printf("[A1] V:%dV I:%.2fA", (int)v1, c1);

    Blynk.virtualWrite(V20, v1);
    Blynk.virtualWrite(V21, c1);
    Blynk.virtualWrite(V22, p1);
    Blynk.virtualWrite(V23, e1);

    if (!isnan(p1) && p1 > 300) {
      Blynk.virtualWrite(V11, 255);  // LED ON
    } else {
      Blynk.virtualWrite(V11, 0);  // LED OFF
    }
  }

  float v2 = pzem2.voltage();
  float c2 = pzem2.current();
  float p2 = pzem2.power();
  float e2 = pzem2.energy();

  if (!isnan(v2)) {
    lcd.setCursor(0, 3);
    lcd.printf("[A2] V:%dV I:%.2fA", (int)v2, c2);

    Blynk.virtualWrite(V24, v2);
    Blynk.virtualWrite(V25, c2);
    Blynk.virtualWrite(V26, p2);
    Blynk.virtualWrite(V27, e2);

    if (!isnan(p2) && p2 > 300) {
      Blynk.virtualWrite(V12, 255);  // LED ON
    } else {
      Blynk.virtualWrite(V12, 0);  // LED OFF
    }
  }
}

// --------------------- ตรวจสอบไฟ ---------------------
void TimerCheckElectricPower() {
  if (currentMode == MODE_MANUAL) return;  // MANUAL ไม่ตรวจสอบไฟ

  int powerState = digitalRead(POWER_DETECT_PIN);

  if (powerState == HIGH) {  // ไฟมา
    Blynk.virtualWrite(V9, 255);
    if (powerLost) {
      Serial.println("✅ ไฟบ้านกลับมาแล้ว (220V มา)");
      if (!tgPowerOnSent) {
        bot.sendMessage(CHAT_ID, "✅ ไฟบ้านกลับมาแล้ว\n⚡ ระบบกลับมาทำงานปกติ", "");
        tgPowerOnSent = true;
        tgPowerOffSent = false;
      }
      powerLost = false;
    }
  } else {  // ไฟดับ
    Blynk.virtualWrite(V9, 0);
    if (!powerLost) {
      Serial.println("⚠️ ไฟบ้านดับ!");
      if (!tgPowerOffSent) {
        bot.sendMessage(CHAT_ID, "⚠️ แจ้งเตือนไฟฟ้าดับ!\n🔌 ตรวจไม่พบไฟ 220V", "");
        tgPowerOffSent = true;
        tgPowerOnSent = false;
      }
      powerLost = true;
    }
  }
}

// --------------------- อ่าน MQ2 / Flame ---------------------
void ReadMQ2() {
  if (currentMode == MODE_MANUAL) return;  // MANUAL ไม่อ่านเซนเซอร์

  int smokeVal = analogRead(MQ2_PIN);
  Serial.print("[MQ2] Smoke: ");
  Serial.print(smokeVal);
  if (smokeVal > 800) {
    lcd.setCursor(19, 0);
    lcd.print("*");
    Blynk.virtualWrite(V7, 0);
    tgSmokeAlert = true;
  } else {
    lcd.setCursor(19, 0);
    lcd.print(" ");
    Blynk.virtualWrite(V7, 255);
    tgSmokeAlert = false;
    tgSmokeSent = false;
  }

  int flameValue = analogRead(FLAME_A0);
  Serial.print(" | Flame: ");
  Serial.println(flameValue);
  if (flameValue < 1500) {
    Blynk.virtualWrite(V8, 0);
    if (!tgFlameSent) {
      bot.sendMessage(CHAT_ID, "🔥 ตรวจพบเปลวไฟ!", "");
      tgFlameSent = true;
    }
  } else {
    Blynk.virtualWrite(V8, 255);
    tgFlameSent = false;
  }
}

// --------------------- อ่าน DHT ---------------------
void ReadSensor() {
  if (currentMode == MODE_MANUAL) return;  // MANUAL ไม่อ่าน DHT

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) return;

  lastTemp = t;
  lastHumi = h;

  lcd.setCursor(0, 1);
  lcd.printf("Temp:%d C Humi:%d %% ", (int)t, (int)h);

  Blynk.virtualWrite(V6, t);
  Blynk.virtualWrite(V5, h);

  tgTempAlert = (t > 32);
  tgHumiAlert = (h < 40 || h > 55);
}

// --------------------- แสดงเวลา ---------------------
void ClockDisplay() {
  char Time[10], date[11];
  sprintf(date, "%02d/%02d/%04d", day(), month(), year() + 543);
  sprintf(Time, "%02d:%02d:%02d", hour(), minute(), second());

  lcd.setCursor(0, 0);
  if (currentMode == MODE_AUTO) {
    lcd.printf("%s      AUTO", Time);
  } else {
    lcd.printf("%s      MANUAL", Time);
  }

  Blynk.virtualWrite(V49, Time);
  Blynk.virtualWrite(V50, date);
}

void updateModeLED() {

  lcd.setCursor(14, 0);

  if (currentMode == MODE_AUTO) {
    lcd.print("AUTO  ");
    Blynk.virtualWrite(V41, 255);  // AUTO LED ON
    Blynk.virtualWrite(V42, 0);    // MANUAL LED OFF
  } 
  else {
    lcd.print("MANUAL");
    Blynk.virtualWrite(V41, 0);    // AUTO LED OFF
    Blynk.virtualWrite(V42, 255);  // MANUAL LED ON
  }
}




// --------------------- ส่ง Telegram ---------------------
void SendTelegramTask() {
  if (currentMode == MODE_MANUAL) return;  // MANUAL ไม่ส่ง Telegram

  if (tgHumiAlert && !tgHumiSent) {
    String msg = "💧 แจ้งเตือนอุณหภูมิความชื้นผิดปกติ\n";
    msg += "🌡 อุณหภูมิ: " + String(lastTemp, 1) + " °C\n";
    msg += "💧 ความชื้น: " + String(lastHumi, 1) + " %";
    bot.sendMessage(CHAT_ID, msg, "");
    tgHumiSent = true;
  }

  if (tgSmokeAlert && !tgSmokeSent) {
    bot.sendMessage(CHAT_ID, "💨 ตรวจพบควันผิดปกติ", "");
    tgSmokeSent = true;
  }

  if (tgFlameAlert && !tgFlameSent) {
    bot.sendMessage(CHAT_ID, "🔥 ตรวจพบเปลวไฟ!", "");
    tgFlameSent = true;
  }

  float ac1Power = pzem1.power();
  if (!isnan(ac1Power) && ac1Power > 300) {
    if (!tgAC1OnSent) {
      String msg = "❄️ เครื่องปรับอากาศ A1 กำลังทำงาน\n";
      msg += "⚡ ใช้พลังงาน: " + String(ac1Power, 1) + " W";
      bot.sendMessage(CHAT_ID, msg, "");
      tgAC1OnSent = true;
      tgAC1OffSent = false;
    }
  } else {
    if (!tgAC1OffSent) {
      bot.sendMessage(CHAT_ID, "⚠️ เครื่องปรับอากาศ A1 หยุดทำงาน หรือไม่มีการใช้พลังงาน", "");
      tgAC1OffSent = true;
      tgAC1OnSent = false;
    }
  }

  float ac2Power = pzem2.power();
  if (!isnan(ac2Power) && ac2Power > 300) {
    if (!tgAC2OnSent) {
      String msg = "❄️ เครื่องปรับอากาศ A2 กำลังทำงาน\n";
      msg += "⚡ ใช้พลังงาน: " + String(ac2Power, 1) + " W";
      bot.sendMessage(CHAT_ID, msg, "");
      tgAC2OnSent = true;
      tgAC2OffSent = false;
    }
  } else {
    if (!tgAC2OffSent) {
      bot.sendMessage(CHAT_ID, "⚠️ เครื่องปรับอากาศ A2 หยุดทำงาน หรือไม่มีการใช้พลังงาน", "");
      tgAC2OffSent = true;
      tgAC2OnSent = false;
    }
  }
}

// --------------------- Setup / Loop ---------------------
BLYNK_CONNECTED() {
  rtc.begin();

  updateModeLED();

  // ⭐ ซิงค์สถานะปุ่ม V100 / V101 ให้ตรงด้วย
  if (currentMode == MODE_AUTO) {
    Blynk.virtualWrite(V100, 1);
    Blynk.virtualWrite(V101, 0);
  } else {
    Blynk.virtualWrite(V100, 0);
    Blynk.virtualWrite(V101, 1);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(POWER_DETECT_PIN, INPUT);
  pinMode(FLAME_A0, INPUT);
  dht.begin();

  currentMode = MODE_AUTO;
  updateModeLED();


  Wire.begin(21, 22);
  keypad.begin(makeKeymap(keys));
  lcd.begin();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(1, 1);
  lcd.print("WELCOME TO SYSTEM");
  delay(2000);
  lcd.clear();
  lcd.setCursor(2, 1);
  lcd.print("INITIALIZING...");
  delay(1500);
  lcd.clear();

  PZSerial.begin(9600, SERIAL_8N1, 16, 17);

  Blynk.begin(auth, ssid, pass, "blynk.en-26.com", 9600);
  setSyncInterval(10 * 60);

  tgClient.setInsecure();

  TimerReadSensor.setInterval(2513L, ReadSensor);
  TimerReadAMP.setInterval(1709L, ReadAMP);
  TimerClockDisplay.setInterval(900L, ClockDisplay);
  TimerReadMQ2.setInterval(503L, ReadMQ2);
  TimerTelegram.setInterval(8011L, SendTelegramTask);
  timerElectricPower.setInterval(997L, TimerCheckElectricPower);
}

void loop() {
  Blynk.run();

  char key = keypad.getKey();
if (key) {

  Serial.print("KEYPAD: ");
  Serial.println(key);

  // เริ่ม Override
  keypadOverride = true;
  keypadOverrideTime = millis();


  if (key == 'A') {   // AUTO
    currentMode = MODE_AUTO;
    updateModeLED();

    // Sync Blynk ปุ่มให้ตรง
    Blynk.virtualWrite(V100, 1);
    Blynk.virtualWrite(V101, 0);

    lcd.setCursor(0,0);
    lcd.print("MODE: AUTO     ");
  }

  else if (key == 'B') {   // MANUAL
    currentMode = MODE_MANUAL;
    updateModeLED();

    Blynk.virtualWrite(V100, 0);
    Blynk.virtualWrite(V101, 1);

    lcd.setCursor(0,0);
    lcd.print("MODE: MANUAL   ");
  }

  else if (key == 'D') {   // คืน Blynk คุม
    keypadOverride = false;

    lcd.setCursor(0,0);
    lcd.print("MODE: BLYNK    ");

    Serial.println("🔓 RESTORE CONTROL TO BLYNK");
  }
}


  // รัน Task ตามโหมด
  if (currentMode == MODE_AUTO) {
    TimerReadSensor.run();
    TimerClockDisplay.run();
    TimerReadAMP.run();
    TimerReadMQ2.run();
    TimerTelegram.run();
    timerElectricPower.run();
  } else if (currentMode == MODE_MANUAL) {
    TimerClockDisplay.run();
    if (keypadOverride && millis() - keypadOverrideTime > KEYPAD_OVERRIDE_TIMEOUT) {
      keypadOverride = false;
      updateModeLED();
      lcd.setCursor(0, 0);
      lcd.print("MODE: BLYNK    ");
      Serial.println("🔓 Blynk control restored");
    }
  }
}
