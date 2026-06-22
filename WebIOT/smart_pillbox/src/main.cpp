/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — main.cpp
 * File dieu phoi chinh: setup() va loop()
 *
 * Thu tu khoi tao (setup):
 *   1. Serial → 2. LittleFS → 3. Wi-Fi → 4. NTP
 *   → 5. Sensor → 6. Alarm → 7. Web Server
 *
 * Vong lap (loop) — TUYET DOI KHONG delay():
 *   1. Wi-Fi reconnect → 2. Sensor → 3. NTP → 4. Alarm
 *
 * ESPAsyncWebServer tu xu ly requests bat dong bo
 * ═══════════════════════════════════════════════════════════
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "config.h"
#include "storage_manager.h"
#include "time_manager.h"
#include "sensor_manager.h"
#include "alarm_manager.h"
#include "web_server_handler.h"
#include "cloud_sync.h"

// ════════════════════════════════════════
// Global
// ════════════════════════════════════════
AsyncWebServer server(80);

// Bien theo doi Wi-Fi reconnect
static unsigned long _lastWifiCheck = 0;
static bool _wifiConnected = false;

// ════════════════════════════════════════
// setup() — Khoi tao he thong theo thu tu
// ════════════════════════════════════════
void setup() {
  // ── 1. Serial ──
  Serial.begin(115200);
  delay(500);  // Cho Serial on dinh (chi delay trong setup)
  Serial.println();
  Serial.println("===============================");
  Serial.println("  === TU THUOC THONG MINH ===  ");
  Serial.println("  Smart Pillbox v1.0 — ESP32   ");
  Serial.println("===============================");
  Serial.println();

  // ── 2. LittleFS (Storage) ──
  if (Storage_Init()) {
    DBGLN("[BOOT] LittleFS: OK");
  } else {
    DBGLN("[BOOT] LittleFS: FAIL — He thong co the hoat dong khong on dinh!");
  }

  // ── 3. Ket noi Wi-Fi ──
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  DBG("[WIFI] Dang ket noi ");
  DBG(WIFI_SSID);
  DBG(" ");

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < WIFI_RETRY_COUNT) {
    delay(WIFI_RETRY_DELAY_MS);
    DBG(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    _wifiConnected = true;
    Serial.println();
    DBG("[WIFI] Da ket noi! IP: ");
    DBGLN(WiFi.localIP().toString());
    DBG("[WIFI] RSSI: ");
    DBG(String(WiFi.RSSI()));
    DBGLN(" dBm");
  } else {
    _wifiConnected = false;
    Serial.println();
    DBGLN("[WIFI] CANH BAO: Khong the ket noi Wi-Fi!");
    DBGLN("[WIFI] He thong chay offline. Se thu lai moi 30 giay.");
  }

  // ── 4. NTP (chi khi co Wi-Fi) ──
  if (_wifiConnected) {
    if (TimeManager_Init()) {
      DBGLN("[BOOT] NTP: OK");
    } else {
      DBGLN("[BOOT] NTP: FAIL — Thoi gian co the khong chinh xac!");
    }
  } else {
    DBGLN("[BOOT] NTP: SKIP (khong co Wi-Fi)");
  }

  // ── 5. Cam bien IR ──
  SensorManager_Init();
  DBGLN("[BOOT] Sensor: 3 cam bien san sang");

  // ── 6. Alarm State Machine ──
  AlarmManager_Init();
  DBGLN("[BOOT] Alarm: State machine san sang");

  // ── 7. Web Server (chi khi co Wi-Fi) ──
  if (_wifiConnected) {
    WebServer_Init(server);
    server.begin();
    DBGLN("[WEB] Server bat dau tren port 80");
    
    // Khoi tao Cloud Sync
    CloudSync_Init();
    DBGLN("[BOOT] Cloud Sync san sang");
  } else {
    DBGLN("[WEB] SKIP — Khong co Wi-Fi");
  }

  // ── Banner hoan tat ──
  Serial.println();
  Serial.println("===============================");
  DBGLN("  He thong san sang!");
  if (_wifiConnected) {
    DBG("  Truy cap: http://");
    DBGLN(WiFi.localIP().toString());
  } else {
    DBGLN("  (Offline mode)");
  }
  Serial.println("===============================");
  Serial.println();
}

// ════════════════════════════════════════
// loop() — Vong lap chinh, NON-BLOCKING
// ════════════════════════════════════════
void loop() {
  // 1. Kiem tra va reconnect Wi-Fi neu mat ket noi
  if (millis() - _lastWifiCheck >= WIFI_RECONNECT_INTERVAL_MS) {
    _lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      DBGLN("[WIFI] Mat ket noi! Dang thu lai...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    } else if (!_wifiConnected) {
      // Vua reconnect thanh cong
      _wifiConnected = true;
      DBG("[WIFI] Da ket noi lai! IP: ");
      DBGLN(WiFi.localIP().toString());

      // Khoi tao NTP va Web Server neu chua
      TimeManager_Init();
      WebServer_Init(server);
      server.begin();
      DBGLN("[WIFI] NTP + Web Server da khoi dong lai");
    }
  }

  // 2. Doc cam bien (voi debounce noi bo)
  //    Goi TRUOC AlarmManager de dam bao edge flags san sang
  SensorManager_Update();

  // 3. Cap nhat NTP (tu throttle moi 1 gio)
  TimeManager_Update();

  // 4. Xu ly state machine bao dong + dieu khien LED/Buzzer
  AlarmManager_Update();

  // 5. Dong bo Cloud
  if (_wifiConnected) {
    CloudSync_PullConfig();
    CloudSync_ProcessRetryLogs();
  }

  // ESPAsyncWebServer tu xu ly requests bat dong bo
  // — khong can goi gi them
}
