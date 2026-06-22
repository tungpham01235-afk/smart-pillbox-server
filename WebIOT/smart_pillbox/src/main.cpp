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

#include "alarm_manager.h"
#include "cloud_sync.h"
#include "config.h"
#include "sensor_manager.h"
#include "storage_manager.h"
#include "time_manager.h"
#include "web_server_handler.h"
#include "wifi_provisioning.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

// ════════════════════════════════════════
// Global
// ════════════════════════════════════════
AsyncWebServer server(80);

// Bien theo doi Wi-Fi reconnect
static unsigned long _lastWifiCheck = 0;
static bool _wifiConnected = false;
static SensorState _prevSensorStates[3] = {SENSOR_ABSENT, SENSOR_ABSENT, SENSOR_ABSENT};

// ════════════════════════════════════════
// setup() — Khoi tao he thong theo thu tu
// ════════════════════════════════════════
void setup() {
  // ── 1. Serial ──
  Serial.begin(115200);
  delay(500); // Cho Serial on dinh (chi delay trong setup)
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

  // ── 3. Khoi tao Wi-Fi Provisioning (STA hoac AP Captive Portal) ──
  WifiProv_Init(server);
  _wifiConnected = (WiFi.status() == WL_CONNECTED);

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
  for (int i = 0; i < 3; i++) {
    _prevSensorStates[i] = SensorManager_GetState(i);
  }
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
  // 1. Duy tri DNS Server trong AP Mode & reset khi can
  WifiProv_Loop();

  // Nếu đang trong chế độ cấu hình Wi-Fi (AP Mode), tạm dừng mọi hoạt động cảm biến, báo động để tránh xung đột LittleFS
  if (WifiProv_IsAPMode()) {
    return;
  }

  // 2. Kiem tra va reconnect Wi-Fi neu mat ket noi
  if (millis() - _lastWifiCheck >= WIFI_RECONNECT_INTERVAL_MS) {
      _lastWifiCheck = millis();
      if (WiFi.status() != WL_CONNECTED) {
        DBGLN("[WIFI] Mat ket noi! Dang thu lai...");
        String ssid, pass, boxId, devKey;
        if (Storage_LoadWifi(ssid, pass, boxId, devKey)) {
          WiFi.disconnect();
          WiFi.begin(ssid.c_str(), pass.c_str());
        }
      } else if (!_wifiConnected) {
        // Vua reconnect thanh cong
        _wifiConnected = true;
        DBG("[WIFI] Da ket noi lai! IP: ");
        DBGLN(WiFi.localIP().toString());

        // Khoi tao NTP va Web Server neu chua
        TimeManager_Init();
        WebServer_Init(server);
        server.begin();
        CloudSync_Init();
        DBGLN("[WIFI] NTP + Web Server + Cloud Sync da khoi dong lai");
      }
    }

  // 3. Doc cam bien (voi debounce noi bo)
  //    Goi TRUOC AlarmManager de dam bao edge flags san sang
  SensorManager_Update();

  // 4. Cap nhat NTP (tu throttle moi 1 gio, chi khi co Wi-Fi va khong o AP mode)
  if (_wifiConnected && !WifiProv_IsAPMode()) {
    TimeManager_Update();
  }

  // 5. Xu ly state machine bao dong + dieu khien LED/Buzzer
  AlarmManager_Update();

  // 6. Dong bo Cloud
  if (_wifiConnected && !WifiProv_IsAPMode()) {
    bool sensorChanged = false;
    for (int i = 0; i < 3; i++) {
      SensorState currentVal = SensorManager_GetState(i);
      if (currentVal != _prevSensorStates[i]) {
        sensorChanged = true;
        _prevSensorStates[i] = currentVal;
      }
    }
    CloudSync_PullConfig(sensorChanged);
    CloudSync_ProcessRetryLogs();
  }

  // ESPAsyncWebServer tu xu ly requests bat dong bo
  // — khong can goi gi them
}
