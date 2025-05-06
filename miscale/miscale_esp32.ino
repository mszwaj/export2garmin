#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Timestamps.h>
#include <Inkplate.h>
#include <time.h>
#include <sntp.h>

// Scale MAC address, please use lowercase letters
#define scale_mac_addr "MiScale_MAC_CHANGE"

// Network details
const char* ssid = "WiFi_ssid_CHANGE";
const char* password = "WiFi_password_CHANGE";

// // time.h details
// const char* ntpServer1 = "pool.ntp.org";
// const char* ntpServer2 = "time.nist.gov";
// const long  gmtOffset_sec = 3600;
// const int   daylightOffset_sec = 3600;

// Initialize Inkplate object
Inkplate display;

// Instantiating object of class Timestamp (time offset is possible in import_data.sh file)
Timestamps ts(0);

// MQTT details
const char* mqtt_server = "mqtt_server_ip_CHANGE";
const int mqtt_port = 1883;
const char* mqtt_userName = "admin";
const char* mqtt_userPass = "mqtt_password_CHANGE";
const char* clientId = "esp32_scale";
const char* mqtt_attributes = "data"; 

String mqtt_clientId = String(clientId);
String mqtt_topic_attributes = String(mqtt_attributes);
String publish_data;

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

char cetTimeStr[25];

int16_t stoi(String input, uint16_t index1) {
    return (int16_t)(strtol(input.substring(index1, index1+2).c_str(), NULL, 16));
}
int16_t stoi2(String input, uint16_t index1) {
    return (int16_t)(strtol((input.substring(index1+2, index1+4) + input.substring(index1, index1+2)).c_str(), NULL, 16));
}

void StartESP32() {
  // Initializing serial port for debugging purposes, version info
  Serial.begin(115200);
  Serial.println();
  Serial.println("===========================================");
  Serial.println("Export 2 Garmin Connect (Inkplate2 edition)");
  Serial.println("===========================================");
  Serial.println();
}

void goToDeepSleep() {
  // Deep sleep for 5 minutes
  Serial.println("* Waiting for next scan, going to sleep");
  esp_sleep_enable_timer_wakeup(5 * 60 * 1000000);
  esp_deep_sleep_start();
}

void displayDraw() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(INKPLATE2_BLACK);
  display.setCursor(0, 0);
  display.println("Export2Garmin");
  display.setTextSize(2);
  display.setTextColor(INKPLATE2_RED);
  display.println(publish_data.c_str());
  display.setTextColor(INKPLATE2_BLACK);

  // Convert C-style string to Arduino String object
  String fullDateTime = String(cetTimeStr);
    
  // Find the space between date and time
  int spacePos = fullDateTime.indexOf(' ');
    
  if (spacePos != -1) {
    // Extract date and time using substring
    String dateStr = fullDateTime.substring(0, spacePos);
    String timeStr = fullDateTime.substring(spacePos + 1);
    display.print("Date: ");
    display.println(dateStr);
    display.print("Time: ");
    display.println(timeStr);
  } else {
    // Fall back to original display if parsing fails
    display.println(cetTimeStr);
  }
  display.display();
}

// Function to convert a UNIX timestamp to CET time string in YYYY-MM-DD HH:MM:SS format
void unixTimestampToCETString(time_t timestamp, char* buffer, size_t bufferSize) {
  struct tm timeinfo;
  
  // Convert timestamp to tm struct according to local timezone (CET)
  localtime_r(&timestamp, &timeinfo);
  
  // Format the time as YYYY-MM-DD HH:MM:SS
  strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

void connectWiFi() {
   int nFailCount = 0;
   Serial.print("* Connecting to WiFi: ");
     while (WiFi.status() != WL_CONNECTED) {
       WiFi.mode(WIFI_STA);
       WiFi.begin(ssid, password);
       WiFi.waitForConnectResult();
     if (WiFi.status() == WL_CONNECTED) {
        Serial.println("connected");
        Serial.print("  IP address: ");
        Serial.println(WiFi.localIP());
     }
     else {
       Serial.print(".");
       delay(200);
       nFailCount++;
       if (nFailCount > 75)
          errorLED_connect();
    }
  }
}

void connectMQTT() {
   int nFailCount = 0;
   connectWiFi();
   Serial.print("* Connecting to MQTT: ");
     while (!mqtt_client.connected()) {
       mqtt_client.setServer(mqtt_server, mqtt_port);
     if (mqtt_client.connect(mqtt_clientId.c_str(),mqtt_userName,mqtt_userPass)) {
       Serial.println("connected");
     }
     else {
       Serial.print(".");
       delay(200);
       nFailCount++;
       if (nFailCount > 75)
          errorLED_connect();
    }  
  }
}

void errorLED_connect() {
  Serial.println("failed");
  goToDeepSleep();
}

void errorLED_scan() {
  Serial.println("* Reading BLE data incomplete, finished BLE scan");
  goToDeepSleep();
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.print("  BLE device found with address: ");
      Serial.print(advertisedDevice.getAddress().toString().c_str());
      if (advertisedDevice.getAddress().toString() == scale_mac_addr) {
        Serial.println(" <= target device");
        BLEScan *pBLEScan = BLEDevice::getScan(); // found what we want, stop now
        pBLEScan->stop();
      }
      else {
        Serial.println(", non-target device");
      }      
   }
};

void ScanBLE() {
  Serial.println("* Starting BLE scan:");
  BLEDevice::init("");
  BLEScan *pBLEScan = BLEDevice::getScan(); //Create new scan.
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false); //Active scan uses more power.
  pBLEScan->setInterval(0x50);
  pBLEScan->setWindow(0x30);
  
  // Scan for 10 seconds
  BLEScanResults foundDevices = pBLEScan->start(10);
  int count = foundDevices.getCount();
  for (int i = 0; i < count; i++) {
    BLEAdvertisedDevice d = foundDevices.getDevice(i);
    if (d.getAddress().toString() != scale_mac_addr)
      continue;
    String hex;
    if (d.haveServiceData()) {
      std::string md = d.getServiceData();
      uint8_t* mdp = (uint8_t*)d.getServiceData().data();
      char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
      hex = pHex;
      free(pHex);
    }
    float Weight = stoi2(hex, 22) * 0.005;
    float Impedance = stoi2(hex, 18);
    if (Impedance > 0) {
      int Unix_time = ts.getTimestampUNIX(stoi2(hex, 4), stoi(hex, 8), stoi(hex, 10), stoi(hex, 12), stoi(hex, 14), stoi(hex, 16));  
      
      // Convert UNIX timestamp to CET time

      unixTimestampToCETString(Unix_time, cetTimeStr, sizeof(cetTimeStr));
      
      Serial.print("UNIX timestamp: ");
      Serial.println(Unix_time);
      Serial.print("CET time: ");
      Serial.println(cetTimeStr);


      // Prepare to send raw values
      publish_data += String(Unix_time);
      publish_data += String(";");
      publish_data += String(Weight, 1);
      publish_data += String(";");
      publish_data += String(Impedance, 0);

      // Send data to MQTT broker and let app figure out the rest
      connectMQTT();
      mqtt_client.publish(mqtt_topic_attributes.c_str(), publish_data.c_str(), true);
      Serial.print("* Publishing MQTT data: ");
      Serial.println(publish_data.c_str());
      displayDraw();
    }
    else {
      errorLED_scan();
    }
  }
}

void setup() {
  display.begin();
  StartESP32();
  ScanBLE();
  goToDeepSleep();
}

void loop() {
}
