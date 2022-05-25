#include <Wire.h>
#include <SPI.h>
#include <NewPing.h>

#include <WiFi.h> 
#include <HTTPClient.h>

// Sign up to Grafana Cloud, and from the portal you can get your username, token and URL for your stack
String grafana_username = "123456";
String grafana_password = "<your-token-from-grafana-cloud>";
String grafana_url = "prometheus-blocks-prod-us-central1.grafana.net";

String DEVICEID = "water-softner-test";

// WIFI credentials for your home router
const char* ssid = "<your-Wifi-SSID>";           
const char* password = "<your-WIFI-Password>"; 

// TX, RX Pins
#define tXPin 17
#define rXPin 16

// Sonar 
#define MAX_DISTANCE 250
#define METRIC_COUNT 15
int distance_measured = -1;
int median_cm = -1;

#define uS_TO_S_FACTOR 1000000  // Conversion factor for micro seconds to seconds 
#define TIME_TO_SLEEP  1800     // 1800 seconds == 30 minutes
RTC_DATA_ATTR int loop_count = 0;

// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

void initWifi() {
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  int wifi_loop_count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    // Restart the device if we can't connect to WiFi after 2 minutes
    wifi_loop_count += 1;
    if (wifi_loop_count > 240) {
      ESP.restart();
    }
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  int httpResponseCode = -1;
  
  Serial.begin(115200);
  delay(1000);

  if (loop_count == 0) {     
    initWifi();

    // Take 15 measurements, and grab the median. Luckily the library does this for us.
    NewPing sonar = NewPing(tXPin, rXPin, MAX_DISTANCE);
    long median_ms = sonar.ping_median(METRIC_COUNT);
    median_cm = sonar.convert_cm(median_ms);
      
    Serial.println("Median cm: " + String(median_cm) + "cm");

    // Only send the metric if it is within expected range, ie less than 50cm. If it is more than 50cm, we're probably not in the salt machine
    if (median_cm > 50) {
      loop_count = 0;
      Serial.println("Distance measured out of range, skipping");
    } else {

      // If for some reason we lost WiFi connection, let's be safe and restart
      if (WiFi.status() != WL_CONNECTED) {
        ESP.restart();
      }      

      // Send to Grafana via InfluxDB Protocol
      HTTPClient http_grafana;   
      // Your Domain name with URL path or IP address with path
      http_grafana.begin("https://" + grafana_username + ":" + grafana_password + "@" + grafana_url + "/api/v1/push/influx/write");
      String POSTtext = "watersoftner,deviceid=" + DEVICEID + " ping_distance=" + String(median_cm);  
      Serial.println("Sending to Grafana Cloud: " + POSTtext);
      httpResponseCode = http_grafana.POST(POSTtext);
      Serial.println("httpResponseCode: " + String(httpResponseCode));
      if (httpResponseCode == -1) {
        ESP.restart();
      }    
    }
  }
  loop_count += 1;

  // Wait for 2 hours, before we can send metrics over
  if (loop_count == 4) {
    loop_count = 0;
  }
  
  // Sleep
  Serial.println("Now going to deep sleep..."); 
  delay(1000);
  setCpuFrequencyMhz(20);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
  esp_deep_sleep_start();
}

void loop() {

}
