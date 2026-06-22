/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — web_server_handler.cpp
 * Implementation: Static files + 8 API endpoints + CORS
 *
 * API Endpoints:
 *   GET    /api/status    → Trang thai realtime
 *   GET    /api/config    → Cau hinh ngan thuoc
 *   POST   /api/config    → Luu cau hinh moi
 *   GET    /api/logs      → Danh sach log
 *   DELETE /api/logs      → Xoa toan bo log
 *   GET    /api/test/{id} → Test alarm ngan id (1-3)
 *   GET    /api/ping      → Kiem tra ket noi
 * ═══════════════════════════════════════════════════════════
 */

#include "web_server_handler.h"
#include "config.h"
#include "storage_manager.h"
#include "time_manager.h"
#include "sensor_manager.h"
#include "alarm_manager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>

// ════════════════════════════════════════
// Helper: Them CORS headers vao response
// ════════════════════════════════════════
static void addCorsHeaders(AsyncWebServerResponse* response) {
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ════════════════════════════════════════
// Helper: Chuyen AlarmState sang String
// ════════════════════════════════════════
static String alarmStateToString(AlarmState state) {
  switch (state) {
    case ALARM_ALARMING:     return "ALARMING";
    case ALARM_ACKNOWLEDGED: return "ACKNOWLEDGED";
    case ALARM_MISSED:       return "MISSED";
    default:                 return "IDLE";
  }
}

// ════════════════════════════════════════
// WebServer_Init — Dang ky tat ca routes
// ════════════════════════════════════════
void WebServer_Init(AsyncWebServer& server) {

  // ──────────────────────────────────────
  // Route 1: Static files tu LittleFS
  // ──────────────────────────────────────
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // ──────────────────────────────────────
  // Route 2: GET /api/status
  // Trang thai realtime cua toan bo he thong
  // ──────────────────────────────────────
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(1024);

    // Thoi gian hien tai
    doc["currentTime"] = TimeManager_GetTimeString();
    doc["currentDate"] = TimeManager_GetDateTimeString().substring(0, 10);
    doc["ntpSynced"]   = TimeManager_IsTimeValid();
    doc["wifiRSSI"]    = WiFi.RSSI();

    // Thong tin tung ngan thuoc
    JsonArray compartments = doc.createNestedArray("compartments");
    for (int i = 0; i < NUM_COMPARTMENTS; i++) {
      JsonObject comp = compartments.createNestedObject();
      comp["id"]                = i + 1;
      comp["medicineName"]      = AlarmManager_GetConfigMedicineName(i);
      comp["scheduleTime"]      = AlarmManager_GetConfigScheduleTime(i);
      comp["mealNote"]          = AlarmManager_GetConfigMealNote(i);
      comp["enabled"]           = AlarmManager_GetConfigEnabled(i);
      comp["isMedicinePresent"] = (SensorManager_GetState(i) == SENSOR_PRESENT);

      String stateStr = alarmStateToString(AlarmManager_GetState(i));
      comp["alarmState"]        = stateStr;
      comp["isAlarmActive"]     = (stateStr == "ALARMING");
    }

    String output;
    serializeJson(doc, output);

    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", output);
    addCorsHeaders(response);
    request->send(response);
  });

  // ──────────────────────────────────────
  // Route 3: GET /api/config
  // Tra ve cau hinh hien tai tu LittleFS
  // ──────────────────────────────────────
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(2048);

    if (!Storage_LoadConfig(doc)) {
      AsyncWebServerResponse* response = request->beginResponse(
        500, "application/json", "{\"success\":false,\"message\":\"Loi doc config\"}");
      addCorsHeaders(response);
      request->send(response);
      return;
    }

    String output;
    serializeJson(doc, output);

    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", output);
    addCorsHeaders(response);
    request->send(response);
  });

  // ──────────────────────────────────────
  // Route 4: POST /api/config
  // Luu cau hinh moi (body = JSON)
  // ──────────────────────────────────────
  server.on("/api/config", HTTP_POST,
    // Handler chinh (rong — xu ly trong onBody)
    [](AsyncWebServerRequest* request) {},
    // onUpload (khong dung)
    NULL,
    // onBody — nhan va xu ly body data
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len,
       size_t index, size_t total) {
      // Chi xu ly khi nhan du body (index == 0, len == total cho body nho)
      String body = String((char*)data).substring(0, len);

      DynamicJsonDocument doc(2048);
      DeserializationError err = deserializeJson(doc, body);

      if (err) {
        AsyncWebServerResponse* response = request->beginResponse(
          400, "application/json", "{\"success\":false,\"message\":\"JSON loi\"}");
        addCorsHeaders(response);
        request->send(response);
        return;
      }

      // Validate: kiem tra co truong "compartments"
      if (!doc.containsKey("compartments")) {
        AsyncWebServerResponse* response = request->beginResponse(
          400, "application/json",
          "{\"success\":false,\"message\":\"Thieu truong compartments\"}");
        addCorsHeaders(response);
        request->send(response);
        return;
      }

      // Luu va reload
      Storage_SaveConfig(doc);
      AlarmManager_ReloadConfig();

      AsyncWebServerResponse* response = request->beginResponse(
        200, "application/json", "{\"success\":true,\"message\":\"Da luu cau hinh\"}");
      addCorsHeaders(response);
      request->send(response);

      DBGLN("[WEB] Config saved via API");
    }
  );

  // ──────────────────────────────────────
  // Route 5: GET /api/logs
  // Tra ve danh sach nhat ky
  // ──────────────────────────────────────
  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(8192);
    Storage_GetLogs(doc);

    String output;
    serializeJson(doc, output);

    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", output);
    addCorsHeaders(response);
    request->send(response);
  });

  // ──────────────────────────────────────
  // Route 6: DELETE /api/logs
  // Xoa toan bo nhat ky
  // ──────────────────────────────────────
  server.on("/api/logs", HTTP_DELETE, [](AsyncWebServerRequest* request) {
    Storage_ClearLogs();

    AsyncWebServerResponse* response = request->beginResponse(
      200, "application/json", "{\"success\":true,\"message\":\"Da xoa nhat ky\"}");
    addCorsHeaders(response);
    request->send(response);

    DBGLN("[WEB] Logs cleared via API");
  });

  // ──────────────────────────────────────
  // Route 7: GET /api/test/{id}
  // Test alarm cho ngan id (1-3)
  // ──────────────────────────────────────
  server.on("/api/test/1", HTTP_GET, [](AsyncWebServerRequest* request) {
    AlarmManager_TriggerTest(0);
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"success\":true,\"message\":\"Dang test ngan 1 trong 10 giay\"}");
    addCorsHeaders(response);
    request->send(response);
  });

  server.on("/api/test/2", HTTP_GET, [](AsyncWebServerRequest* request) {
    AlarmManager_TriggerTest(1);
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"success\":true,\"message\":\"Dang test ngan 2 trong 10 giay\"}");
    addCorsHeaders(response);
    request->send(response);
  });

  server.on("/api/test/3", HTTP_GET, [](AsyncWebServerRequest* request) {
    AlarmManager_TriggerTest(2);
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"success\":true,\"message\":\"Dang test ngan 3 trong 10 giay\"}");
    addCorsHeaders(response);
    request->send(response);
  });

  // ──────────────────────────────────────
  // Route 8: GET /api/ping
  // Kiem tra ket noi + uptime
  // ──────────────────────────────────────
  server.on("/api/ping", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = "{\"status\":\"ok\",\"uptime\":" + String(millis()) + "}";
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
    addCorsHeaders(response);
    request->send(response);
  });

  // ──────────────────────────────────────
  // Route 9: OPTIONS (CORS preflight) + 404
  // ──────────────────────────────────────
  server.onNotFound([](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_OPTIONS) {
      AsyncWebServerResponse* response = request->beginResponse(200);
      addCorsHeaders(response);
      request->send(response);
    } else {
      request->send(404, "text/plain", "Not Found");
    }
  });

  DBGLN("[WEB] Tat ca routes da dang ky");
}
