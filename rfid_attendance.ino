#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <UniversalTelegramBot.h>
#include <ESP8266WebServer.h>

#define SS_PIN D4
#define RST_PIN D3
#define BUZZER_PIN D0

MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
ESP8266WebServer server(80);

const char* ssid = "Airtel_Vishu";
const char* password = "18162830@Vaibhav";

#define BOT_TOKEN "8101793708:AAHPRDThFxCwjS0YWr0A1p6o5eMojWrNySQ"
#define CHAT_ID "7520122171"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);

String studentUIDs[4] = {
  "37 00 01 04",
  "D9 28 00 04",
  "45 3A 00 04",
  "D2 5E 27 02"
};

String studentNames[4] = {
  "Navdisha",
  "Ridhima",
  "Rahul",
  "Sneha"
};

bool present[4] = {false, false, false, false};
int presentCount = 0;
unsigned long classStartTime = 0;
bool classTimerStarted = false;
bool classStarted = false;
String logData = "";

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  lcd.init();
  lcd.backlight();
  pinMode(BUZZER_PIN, OUTPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }

  Serial.println("");
  Serial.println("Connected to WiFi");
  Serial.println("IP Address: " + WiFi.localIP().toString());

  secured_client.setInsecure();
  timeClient.begin();

  server.on("/", handleRoot);
  server.begin();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(2000);
}

void loop() {
  server.handleClient();
  timeClient.update();

  if (classStarted && millis() - classStartTime >= 5 * 60000) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Class Done!");
    lcd.setCursor(0, 1);
    lcd.print("Ready Next");
    bot.sendMessage(CHAT_ID, "📘 Class has ended!\n⏹️ Ready for next class.", "");
    resetSystem();
    delay(3000);
  }

  if (!classStarted && mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uid = getUID();
    Serial.println("Card UID: " + uid);

    int index = checkUID(uid);

    if (index != -1 && !present[index]) {
      present[index] = true;
      presentCount++;
      String dateTime = timeClient.getFormattedTime();

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Welcome ");
      lcd.print(studentNames[index]);
      lcd.setCursor(0, 1);
      lcd.print(dateTime);

      beep(1);
      bot.sendMessage(CHAT_ID, "📚 " + studentNames[index] + " entered.\n🆔 UID: " + uid + "\n🕒 Time: " + dateTime, "");
      logData += studentNames[index] + "," + uid + "," + dateTime + ",Present<br>";

      if (presentCount == 4 && !classTimerStarted) {
        classStartTime = millis();
        classTimerStarted = true;
        bot.sendMessage(CHAT_ID, "✅ All students present.\n⏳ Class starts in 5 min.", "");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Class in 5 min");
        lcd.setCursor(0, 1);
        lcd.print("Countdown...");
      }
    } else if (classStarted) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Entry Blocked");
      lcd.setCursor(0, 1);
      lcd.print("Class in progress");
      bot.sendMessage(CHAT_ID, "⛔ Entry not allowed!\nClass already started.", "");
      beep(2);
    }
    mfrc522.PICC_HaltA();
  }

  if (classTimerStarted && !classStarted) {
    unsigned long elapsed = millis() - classStartTime;

    if (elapsed >= 3 * 60000 && elapsed < 3.1 * 60000) {
      bot.sendMessage(CHAT_ID, "⚠️ Last 2 min before class starts!", "");
      beep(2);
    }

    if (elapsed >= 5 * 60000) {
      bot.sendMessage(CHAT_ID, "🚀 Class Started!", "");
      beep(3);
      classStarted = true;
    } else {
      lcd.setCursor(0, 0);
      lcd.print("Starts in:");
      int remaining = 5 - (elapsed / 60000);
      lcd.setCursor(0, 1);
      lcd.print(remaining); lcd.print(" min left    ");
    }
  }
}

String getUID() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    uid += String(mfrc522.uid.uidByte[i], HEX);
    if (i < mfrc522.uid.size - 1) uid += " ";
  }
  uid.toUpperCase();
  return uid;
}

int checkUID(String uid) {
  for (int i = 0; i < 4; i++) {
    if (uid == studentUIDs[i]) return i;
  }
  return -1;
}

void handleRoot() {
  String page = "<html><head><title>Attendance Log</title></head><body>";
  page += "<h2>📋 Attendance Record</h2><table border='1'><tr><th>Name</th><th>UID</th><th>Time</th><th>Status</th></tr>";

  for (int i = 0; i < 4; i++) {
    if (present[i]) {
      page += "<tr><td>" + studentNames[i] + "</td><td>" + studentUIDs[i] + "</td><td>Recorded</td><td>Present</td></tr>";
    } else {
      page += "<tr><td>" + studentNames[i] + "</td><td>" + studentUIDs[i] + "</td><td>-</td><td>Absent</td></tr>";
    }
  }

  page += "</table><br><a href='data:text/csv;charset=utf-8," + logData + "' download='attendance.csv'>⬇️ Download CSV</a>";
  page += "</body></html>";
  server.send(200, "text/html", page);
}

void beep(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH); delay(200);
    digitalWrite(BUZZER_PIN, LOW); delay(200);
  }
}

void resetSystem() {
  for (int i = 0; i < 4; i++) present[i] = false;
  presentCount = 0;
  classTimerStarted = false;
  classStarted = false;
  logData = "";
}
