#define BLYNK_TEMPLATE_ID "TMPL31HLGi3-p"
#define BLYNK_TEMPLATE_NAME "Battery Intelligence"
#define BLYNK_AUTH_TOKEN "mDpz3jCucC_3Fvw6R5D4SS2AIgkg0FZf"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

char ssid[] = "Wokwi-GUEST";
char pass[] = "";

LiquidCrystal_I2C lcd(0x27,16,2);
BlynkTimer timer;

#define POT 34

float voltage, lastVoltage = 0;
int soc;
String risk, lastRisk = "";

void sendData()
{
  voltage = 10.5 + analogRead(POT) * 2.1 / 4095.0;
  soc = constrain((voltage - 10.5) * 100 / 2.1, 0, 100);

  if(voltage < 11.0)
    risk = "CRITICAL";
  else if(voltage < 11.5)
    risk = "WARNING";
  else
    risk = "NORMAL";

  // LCD
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("BAT:");
  lcd.print(voltage,2);
  lcd.print("V");

  lcd.setCursor(0,1);
  lcd.print("SOC:");
  lcd.print(soc);
  lcd.print("% ");
  lcd.print(risk);

  // Send only when voltage/risk changes
  if(abs(voltage-lastVoltage) > 0.05 || risk != lastRisk)
  {
    Blynk.virtualWrite(V0, voltage);
    Blynk.virtualWrite(V2, soc);
    Blynk.virtualWrite(V4, risk);
    Blynk.virtualWrite(V11, voltage);

    if(risk == "NORMAL")
    {
      Blynk.virtualWrite(V5, "HEALTHY");
      Blynk.virtualWrite(V6, "NO FAULTS");
      Blynk.virtualWrite(V7, "SYSTEM NORMAL");
      Blynk.virtualWrite(V10, "NORMAL");
    }
    else if(risk == "WARNING")
    {
      Blynk.virtualWrite(V5, "VOLTAGE LOW");
      Blynk.virtualWrite(V6, "LOW VOLTAGE");
      Blynk.virtualWrite(V7, "MONITOR BATTERY");
      Blynk.virtualWrite(V10, "WARNING");
    }
    else
    {
      Blynk.virtualWrite(V5, "CRITICAL VOLTAGE");
      Blynk.virtualWrite(V6, "CRITICAL LOW VOLTAGE");
      Blynk.virtualWrite(V7, "CHECK BATTERY");
      Blynk.virtualWrite(V10, "CRITICAL");
    }

    lastVoltage = voltage;
    lastRisk = risk;
  }

  Blynk.virtualWrite(V8, Blynk.connected() ? "ONLINE" : "OFFLINE");
  Blynk.virtualWrite(V9, WiFi.RSSI());
}

void setup()
{
  Serial.begin(115200);

  Wire.begin(21,22);

  lcd.init();
  lcd.backlight();

  lcd.print("Battery System");
  delay(1000);

  WiFi.begin(ssid,pass);
  Blynk.config(BLYNK_AUTH_TOKEN);

  timer.setInterval(2000L, sendData);
}

void loop()
{
  if(WiFi.status() == WL_CONNECTED)
  {
    if(!Blynk.connected())
      Blynk.connect(1000);

    Blynk.run();
  }

  timer.run();
}
