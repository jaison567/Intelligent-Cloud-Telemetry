#define BLYNK_TEMPLATE_ID "TMPL3e5ZbhYym"
#define BLYNK_TEMPLATE_NAME "Intelligent Cloud Telemetry"
#define BLYNK_AUTH_TOKEN "PMSgILnJdzsGy9_TCnnv11b67Diqzje8"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

char ssid[] = "Wokwi-GUEST";
char pass[] = "";

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define VOLTAGE_PIN     34
#define CURRENT_PIN     35
#define TEMPERATURE_PIN 32

#define LOW_VOLTAGE_LIMIT   11.0
#define HIGH_VOLTAGE_LIMIT  14.6

#define MAX_CURRENT          40.0
#define MAX_TEMPERATURE      60.0

// ==========================================================
// TIMERS
// ==========================================================

unsigned long lastSensorRead = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastBlynkCheck = 0;
unsigned long lastSignalCheck = 0;
unsigned long lastLCDUpdate = 0;

const unsigned long SENSOR_INTERVAL = 500;
const unsigned long WIFI_INTERVAL = 5000;
const unsigned long BLYNK_INTERVAL = 5000;
const unsigned long SIGNAL_INTERVAL = 10000;
const unsigned long LCD_INTERVAL = 1000;

// ==========================================================
// SENSOR VARIABLES
// ==========================================================

float voltage = 0.0;
float current = 0.0;
float temperature = 0.0;

int batteryPercentage = 0;

// ==========================================================
// SYSTEM STATE
// ==========================================================

String systemState = "STARTING";
String previousState = "STARTING";

bool wifiConnectedBefore = false;
bool blynkConnectedBefore = false;

int wifiRSSI = -100;
int wifiSignal = 0;
int previousSignal = -1;


//----- EVENT QUEUE----//
#define QUEUE_SIZE 20

struct TelemetryEvent
{
  float voltage;
  float current;
  float temperature;

  int battery;
  int rssi;
  int signal;

  String state;
  String reason;
};

TelemetryEvent eventQueue[QUEUE_SIZE];

int queueHead = 0;
int queueTail = 0;
int queueCount = 0;

void addEvent(String reason)
{
  TelemetryEvent event;

  event.voltage = voltage;
  event.current = current;
  event.temperature = temperature;

  event.battery = batteryPercentage;

  event.rssi = wifiRSSI;
  event.signal = wifiSignal;

  event.state = systemState;
  event.reason = reason;

  // If queue is full, remove oldest event
  if (queueCount >= QUEUE_SIZE)
  {
    queueHead = (queueHead + 1) % QUEUE_SIZE;
    queueCount--;
  }

  eventQueue[queueTail] = event;

  queueTail = (queueTail + 1) % QUEUE_SIZE;
  queueCount++;

  Serial.print("EVENT QUEUED: ");
  Serial.println(reason);
}

// ==========================================================
// SEND EVENT TO BLYNK
// ==========================================================

void sendEvent(TelemetryEvent &event)
{
  if (!Blynk.connected())
  {
    return;
  }

  // Sensor data
  Blynk.virtualWrite(V0, event.voltage);
  Blynk.virtualWrite(V1, event.current);
  Blynk.virtualWrite(V2, event.temperature);
  Blynk.virtualWrite(V3, event.battery);

  // System state
  Blynk.virtualWrite(V4, event.state);

  // Event reason
  Blynk.virtualWrite(V5, event.reason);

  // Voltage status
  if (event.voltage < LOW_VOLTAGE_LIMIT)
  {
    Blynk.virtualWrite(V6, "LOW VOLTAGE");
  }
  else if (event.voltage > HIGH_VOLTAGE_LIMIT)
  {
    Blynk.virtualWrite(V6, "HIGH VOLTAGE");
  }
  else
  {
    Blynk.virtualWrite(V6, "NORMAL");
  }

  // WiFi signal
  Blynk.virtualWrite(V7, event.signal);
  Blynk.virtualWrite(V8, event.rssi);

  // Cloud status
  Blynk.virtualWrite(V9, "CONNECTED");

  // Fault status
  if (event.state == "NORMAL")
  {
    Blynk.virtualWrite(V10, "SYSTEM OK");
  }
  else
  {
    Blynk.virtualWrite(V10, "FAULT");
  }

  // Detailed status
  Blynk.virtualWrite(V11, event.state);

  Serial.print("EVENT SENT: ");
  Serial.println(event.reason);
}

// ==========================================================
// SYNCHRONIZE QUEUED EVENTS
// ==========================================================

void synchronizeQueue()
{
  if (!Blynk.connected())
  {
    return;
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("SYNCHRONIZING EVENT QUEUE");
  Serial.print("Events waiting: ");
  Serial.println(queueCount);
  Serial.println("================================");

  while (queueCount > 0 && Blynk.connected())
  {
    TelemetryEvent event = eventQueue[queueHead];

    sendEvent(event);

    queueHead = (queueHead + 1) % QUEUE_SIZE;
    queueCount--;

    Blynk.run();

    delay(10);
  }

  if (queueCount == 0)
  {
    queueHead = 0;
    queueTail = 0;

    Serial.println("QUEUE SYNCHRONIZED");
  }
}

// ==========================================================
// UPDATE WIFI SIGNAL
// ==========================================================

void updateWiFiSignal()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    wifiRSSI = WiFi.RSSI();

    // Convert RSSI to approximate percentage
    wifiSignal = constrain(
      2 * (wifiRSSI + 100),
      0,
      100
    );
  }
  else
  {
    wifiRSSI = -100;
    wifiSignal = 0;
  }

  Serial.print("WiFi RSSI: ");
  Serial.print(wifiRSSI);

  Serial.print(" dBm | Signal: ");
  Serial.print(wifiSignal);

  Serial.println("%");
}

// ==========================================================
// WIFI HANDLER
// ==========================================================

void handleWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    if (!wifiConnectedBefore)
    {
      wifiConnectedBefore = true;

      Serial.println();
      Serial.println("==============================");
      Serial.println("WIFI CONNECTED");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      Serial.println("==============================");
    }

    return;
  }

  if (wifiConnectedBefore)
  {
    wifiConnectedBefore = false;

    Serial.println();
    Serial.println("WIFI DISCONNECTED");
    Serial.println("SYSTEM CONTINUES LOCALLY");
  }

  if (millis() - lastWiFiCheck >= WIFI_INTERVAL)
  {
    lastWiFiCheck = millis();

    Serial.println("Trying WiFi reconnect...");

    WiFi.disconnect();
    WiFi.begin(ssid, pass);
  }
}

// ==========================================================
// BLYNK HANDLER
// ==========================================================

void handleBlynk()
{
  // Blynk requires WiFi
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  // Already connected
  if (Blynk.connected())
  {
    if (!blynkConnectedBefore)
    {
      blynkConnectedBefore = true;

      Serial.println();
      Serial.println("==============================");
      Serial.println("BLYNK CLOUD CONNECTED");
      Serial.println("==============================");

      // Send events collected while offline
      synchronizeQueue();
    }

    Blynk.run();

    return;
  }

  // Blynk disconnected
  if (blynkConnectedBefore)
  {
    blynkConnectedBefore = false;

    Serial.println();
    Serial.println("BLYNK CLOUD DISCONNECTED");
    Serial.println("EVENTS WILL BE QUEUED");
  }

  // Try reconnecting
  if (millis() - lastBlynkCheck >= BLYNK_INTERVAL)
  {
    lastBlynkCheck = millis();

    Serial.println("Trying Blynk reconnect...");

    Blynk.connect(1000);
  }
}

// ==========================================================
// READ SENSORS
// ==========================================================

void readSensors()
{
  int rawVoltage = analogRead(VOLTAGE_PIN);
  int rawCurrent = analogRead(CURRENT_PIN);
  int rawTemperature = analogRead(TEMPERATURE_PIN);

  // --------------------------------------------------------
  // VOLTAGE
  // --------------------------------------------------------

  voltage = (rawVoltage * 16.0) / 4095.0;

  current = ((rawCurrent * 100.0) / 4095.0) - 50.0;

  temperature = ((rawTemperature * 140.0) / 4095.0) - 40.0;

  batteryPercentage =
    constrain(
      ((voltage - 11.0) * 100.0) / 3.4,
      0,
      100
    );

  String newState;

  if (voltage < LOW_VOLTAGE_LIMIT)
  {
    newState = "LOW VOLTAGE";
  }
  else if (voltage > HIGH_VOLTAGE_LIMIT)
  {
    newState = "HIGH VOLTAGE";
  }
  else if (temperature > MAX_TEMPERATURE)
  {
    newState = "OVER TEMPERATURE";
  }
  else if (abs(current) > MAX_CURRENT)
  {
    newState = "OVER CURRENT";
  }
  else
  {
    newState = "NORMAL";
  }

  // --------------------------------------------------------
  // STATE CHANGE DETECTION
  // --------------------------------------------------------

  if (newState != systemState)
  {
    previousState = systemState;

    systemState = newState;

    Serial.println();
    Serial.println("******** STATE CHANGE ********");

    Serial.print("Previous: ");
    Serial.println(previousState);

    Serial.print("New: ");
    Serial.println(systemState);

    Serial.println("*******************************");

    addEvent("STATE CHANGE");
  }

  // First reading
  static bool firstReading = true;

  if (firstReading)
  {
    firstReading = false;

    systemState = newState;
    previousState = newState;

    addEvent("INITIAL DATA");
  }
}

// ==========================================================
// PRINT SENSOR DATA
// ==========================================================

void printSensorData()
{
  Serial.println("--------------------------------");

  Serial.print("Voltage     : ");
  Serial.print(voltage, 2);
  Serial.println(" V");

  Serial.print("Current     : ");
  Serial.print(current, 2);
  Serial.println(" A");

  Serial.print("Temperature : ");
  Serial.print(temperature, 2);
  Serial.println(" C");

  Serial.print("Battery     : ");
  Serial.print(batteryPercentage);
  Serial.println(" %");

  Serial.print("State       : ");
  Serial.println(systemState);

  Serial.print("RSSI        : ");
  Serial.print(wifiRSSI);
  Serial.println(" dBm");

  Serial.print("Queue       : ");
  Serial.println(queueCount);

  Serial.println("--------------------------------");
}

// ==========================================================
// LCD
// ==========================================================

void updateLCD()
{
  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("V:");
  lcd.print(voltage, 1);

  lcd.print(" I:");
  lcd.print(current, 1);

  lcd.setCursor(0, 1);

  if (systemState == "NORMAL")
  {
    lcd.print("SYSTEM OK");
  }
  else if (systemState == "LOW VOLTAGE")
  {
    lcd.print("LOW VOLTAGE");
  }
  else if (systemState == "HIGH VOLTAGE")
  {
    lcd.print("HIGH VOLTAGE");
  }
  else if (systemState == "OVER TEMPERATURE")
  {
    lcd.print("OVER TEMP");
  }
  else if (systemState == "OVER CURRENT")
  {
    lcd.print("OVER CURRENT");
  }
}

// ==========================================================
// SETUP
// ==========================================================

void setup()
{
  Serial.begin(115200);

  delay(500);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" INTELLIGENT CLOUD TELEMETRY");
  Serial.println("========================================");

  // --------------------------------------------------------
  // ADC
  // --------------------------------------------------------

  analogReadResolution(12);

  // --------------------------------------------------------
  // LCD
  // --------------------------------------------------------

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Cloud Telemetry");

  lcd.setCursor(0, 1);
  lcd.print("Starting...");

   
  // WIFI //
  WiFi.mode(WIFI_STA);

  Serial.println("Starting WiFi...");

  WiFi.begin(ssid, pass);

  //------- BLYNK--------//
  Blynk.config(BLYNK_AUTH_TOKEN);

  // Try initial Blynk connection
  Serial.println("Trying Blynk Cloud connection...");

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 10000)
  {
    delay(100);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi connected!");

    if (Blynk.connect(10000))
    {
      blynkConnectedBefore = true;

      Serial.println("==============================");
      Serial.println("BLYNK CONNECTED SUCCESSFULLY");
      Serial.println("==============================");
    }
    else
    {
      Serial.println("Blynk not connected yet.");
      Serial.println("Automatic reconnect enabled.");
    }
  }
  else
  {
    Serial.println("WiFi not connected yet.");
    Serial.println("Automatic reconnect enabled.");
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");

  delay(500);
}

// ==========================================================
// MAIN LOOP
// ==========================================================

void loop()
{
  unsigned long now = millis();

  // --------------------------------------------------------
  // 1. WIFI
  // --------------------------------------------------------

  handleWiFi();

  handleBlynk();

  if (now - lastSensorRead >= SENSOR_INTERVAL)
  {
    lastSensorRead = now;

    readSensors();

    printSensorData();
  }


  if (now - lastSignalCheck >= SIGNAL_INTERVAL)
  {
    lastSignalCheck = now;

    updateWiFiSignal();

    // Generate an event only when signal changes significantly
    if (previousSignal == -1)
    {
      previousSignal = wifiSignal;
    }
    else if (abs(wifiSignal - previousSignal) >= 20)
    {
      previousSignal = wifiSignal;

      addEvent("SIGNAL QUALITY CHANGE");
    }
  }

  if (now - lastLCDUpdate >= LCD_INTERVAL)
  {
    lastLCDUpdate = now;

    updateLCD();
  }

  if (Blynk.connected())
  {
    Blynk.run();
  }
}
