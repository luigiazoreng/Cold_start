#include <Arduino.h>
#include "DHT.h"
#include <esp_sleep.h>
#include <time.h>
// Pin and sensor definitions
#define DHTPIN 25 // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11
#define TRIGGER_PIN 26            // Pin to detect the trigger
#define OUTPUT_PIN 27             // Output pin
#define uS_TO_S_FACTOR 1000000ULL // Conversion factor for microseconds to seconds
// DHT sensor object
DHT dht(DHTPIN, DHTTYPE);

// RTC variable to keep track of boot count during deep sleep
RTC_DATA_ATTR int bootCount = 0;
// Variables to store RTC time
RTC_DATA_ATTR time_t sleep_enter_time = 0;
RTC_DATA_ATTR time_t wakeup_time = 0;
RTC_DATA_ATTR struct tm tm_struct = {0};

bool hasActivated = false; // State of activation

void logic();
void print_wakeup_reason();
void set_time();
void printLocalTime();

void setup()
{
  // Initialize Serial
  Serial.begin(9600);
  set_time();
  // Pin setup
  pinMode(TRIGGER_PIN, INPUT_PULLDOWN);
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, HIGH); // Keep OUTPUT_PIN HIGH initially

  // Print wake-up reason
  print_wakeup_reason();
  // Increment boot count
  bootCount++;
  Serial.println("Boot number: " + String(bootCount));
  wakeup_time = time(nullptr); // Get current time (seconds since epoch)
  printLocalTime();
  //  Calculate and print time spent in sleep if not the first wake-up
  if (sleep_enter_time > 0)
  {
    Serial.print("Time spent in sleep (seconds): ");
    Serial.println(difftime(wakeup_time, sleep_enter_time));
  }
  // Set up wake-up on the trigger pin (go to sleep when TRIGGER_PIN is HIGH)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_26, 0); // Wake up when TRIGGER_PIN goes HIGH

  // Initialize DHT sensor
  dht.begin();
  // Call main logic
  logic();
  // Record the current time before going to deep sleep
  Serial.println("TRIGGER_PIN is HIGH, entering deep sleep.");
  // Enter deep sleep mode
  sleep_enter_time = time(nullptr); // Get current time in seconds before sleep
  Serial.flush();
  esp_deep_sleep_start();
}

void loop()
{
  // Main loop does nothing, all logic happens in setup and wakes up
}

void logic()
{
  // Read the trigger pin
  while (digitalRead(TRIGGER_PIN) == LOW)
  {
    // Delay for stability
    delay(2000);

    // Reading humidity and temperature
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t))
    {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }

    float hic = dht.computeHeatIndex(t, h, false);

    // Print results
    Serial.print(F("Humidity: "));
    Serial.print(h);
    Serial.print(F("%  Temperature: "));
    Serial.print(t);
    Serial.print(F("°C  Heat index: "));
    Serial.print(hic);
    Serial.println(F("°C"));

    if (t < 20 && !hasActivated &&
        (difftime(wakeup_time, sleep_enter_time) < 300 || difftime(wakeup_time, sleep_enter_time) > 21600))
    {
      Serial.println("Acionado rele de partida a frio.");
      digitalWrite(OUTPUT_PIN, LOW); // Activate the output
      delay(300);                    // Keep output LOW for 500ms
      digitalWrite(OUTPUT_PIN, HIGH);
      hasActivated = true;
    }
  }
}

void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  String reason = "";

  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    reason = "Wakeup caused by external signal using RTC_IO";
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    reason = "Wakeup caused by external signal using RTC_CNTL";
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    reason = "Wakeup caused by timer";
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    reason = "Wakeup caused by touchpad";
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    reason = "Wakeup caused by ULP program";
    break;
  default:
    reason = "Wakeup was not caused by deep sleep";
    break;
  }

  Serial.println(reason);
}

void set_time()
{
  if (!getLocalTime(&tm_struct, 5000))
  {
    Serial.println("Failed to synchronize time, using default");
    tm_struct.tm_year = 2024 - 1900; // Set the year manually (e.g., 2022)
    tm_struct.tm_mon = 9 - 1;        // Set the month (e.g., September)
    tm_struct.tm_mday = 22;          // Set the day (e.g., 22)
    tm_struct.tm_hour = 12;          // Set the hour (e.g., 14:00)
    tm_struct.tm_min = 0;
    tm_struct.tm_sec = 0;
    time_t t = mktime(&tm_struct);
    timeval now = {.tv_sec = t};
    settimeofday(&now, 0); // Manually set RTC time
  }
}
void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}