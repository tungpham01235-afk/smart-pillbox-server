/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — cloud_sync.cpp
 * Triển khai logic đồng bộ hóa hai chiều (HTTP Client) & gửi bù logs
 * ═══════════════════════════════════════════════════════════
 */

#include "cloud_sync.h"
#include "config.h"
#include "storage_manager.h"
#include "alarm_manager.h"
#include "sensor_manager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

static unsigned long _lastConfigPull = 0;
static unsigned long _lastLogRetry = 0;

void CloudSync_Init() {
  _lastConfigPull = millis();
  _lastLogRetry = millis();
  
  // Tạo file offline_sync.json nếu chưa tồn tại
  if (!LittleFS.exists("/offline_sync.json")) {
    File file = LittleFS.open("/offline_sync.json", "w");
    if (file) {
      file.print("[]");
      file.close();
    }
  }
}

// Hàm phụ để lưu log offline vào LittleFS khi đồng bộ thất bại
static void saveLogOffline(int compartmentId, const String& status) {
  DynamicJsonDocument doc(1024);
  
  File fileRead = LittleFS.open("/offline_sync.json", "r");
  if (fileRead) {
    DeserializationError err = deserializeJson(doc, fileRead);
    fileRead.close();
    if (err) {
      doc.clear();
      doc.to<JsonArray>();
    }
  } else {
    doc.to<JsonArray>();
  }

  JsonArray array = doc.as<JsonArray>();
  
  // Thêm log offline mới
  JsonObject obj = array.createNestedObject();
  obj["compartmentId"] = compartmentId;
  obj["status"] = status;

  File fileWrite = LittleFS.open("/offline_sync.json", "w");
  if (fileWrite) {
    serializeJson(doc, fileWrite);
    fileWrite.close();
    DBGLN("[CLOUD] Đã lưu log offline vào LittleFS do mất kết nối.");
  }
}

bool CloudSync_SendLog(int compartmentId, const String& status) {
  if (WiFi.status() != WL_CONNECTED) {
    saveLogOffline(compartmentId, status);
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, String(CLOUD_SERVER_URL) + "/api/logs/sync");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-device-key", CLOUD_DEVICE_KEY);

  StaticJsonDocument<256> doc;
  doc["boxId"] = CLOUD_BOX_ID;
  doc["compartmentId"] = compartmentId;
  doc["status"] = status;

  String jsonStr;
  serializeJson(doc, jsonStr);

  int httpCode = http.POST(jsonStr);
  bool success = false;

  if (httpCode == 200 || httpCode == 201) {
    DBG("[CLOUD] Đồng bộ log khay ");
    DBG(compartmentId);
    DBGLN(" thành công!");
    success = true;
  } else {
    DBG("[CLOUD] Lỗi gửi log (HTTP Code: ");
    DBG(httpCode);
    DBGLN("). Lưu offline...");
    saveLogOffline(compartmentId, status);
  }

  http.end();
  return success;
}

void CloudSync_ProcessRetryLogs() {
  if (WiFi.status() != WL_CONNECTED) return;

  // Giới hạn tần suất gửi bù log để tránh nghẽn mạng
  if (millis() - _lastLogRetry < CLOUD_LOG_RETRY_INTERVAL) return;
  _lastLogRetry = millis();

  if (!LittleFS.exists("/offline_sync.json")) return;

  DynamicJsonDocument doc(4096);
  File fileRead = LittleFS.open("/offline_sync.json", "r");
  if (!fileRead) return;

  DeserializationError err = deserializeJson(doc, fileRead);
  fileRead.close();
  if (err || !doc.is<JsonArray>() || doc.as<JsonArray>().size() == 0) return;

  JsonArray array = doc.as<JsonArray>();
  DBGLN("[CLOUD] Phát hiện logs offline chưa gửi. Đang tiến hành gửi bù...");

  // Tạo mảng mới chứa các log gửi thất bại để lưu lại
  DynamicJsonDocument failedDoc(4096);
  JsonArray failedArray = failedDoc.to<JsonArray>();

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  bool hasSuccess = false;

  for (JsonObject logObj : array) {
    int compartmentId = logObj["compartmentId"];
    String status = logObj["status"].as<String>();

    http.begin(client, String(CLOUD_SERVER_URL) + "/api/logs/sync");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-device-key", CLOUD_DEVICE_KEY);

    StaticJsonDocument<256> postDoc;
    postDoc["boxId"] = CLOUD_BOX_ID;
    postDoc["compartmentId"] = compartmentId;
    postDoc["status"] = status;

    String jsonStr;
    serializeJson(postDoc, jsonStr);

    int httpCode = http.POST(jsonStr);
    if (httpCode == 200 || httpCode == 201) {
      DBG("[CLOUD] Gửi bù log khay ");
      DBG(compartmentId);
      DBGLN(" thành công!");
      hasSuccess = true;
    } else {
      // Gửi thất bại, giữ lại để gửi sau
      failedArray.add(logObj);
    }
    http.end();
  }

  // Ghi lại mảng logs chưa gửi được xuống LittleFS
  File fileWrite = LittleFS.open("/offline_sync.json", "w");
  if (fileWrite) {
    serializeJson(failedDoc, fileWrite);
    fileWrite.close();
  }
}

void CloudSync_PullConfig() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (millis() - _lastConfigPull < CLOUD_SYNC_INTERVAL_MS) return;
  _lastConfigPull = millis();

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(CLOUD_SERVER_URL) + "/api/device/config?boxId=" + String(CLOUD_BOX_ID);
  url += "&s1=" + String(SensorManager_GetState(0) == SENSOR_PRESENT ? "true" : "false");
  url += "&s2=" + String(SensorManager_GetState(1) == SENSOR_PRESENT ? "true" : "false");
  url += "&s3=" + String(SensorManager_GetState(2) == SENSOR_PRESENT ? "true" : "false");
  
  http.begin(client, url);
  http.addHeader("x-device-key", CLOUD_DEVICE_KEY);

  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    
    DynamicJsonDocument cloudDoc(2048);
    DeserializationError err = deserializeJson(cloudDoc, payload);
    
    if (err || !cloudDoc["success"]) {
      http.end();
      return;
    }

    JsonArray cloudComps = cloudDoc["compartments"].as<JsonArray>();
    if (cloudComps.size() == 0) {
      http.end();
      return;
    }

    // Load cấu hình hiện tại ở LittleFS để đối chiếu
    DynamicJsonDocument localDoc(2048);
    if (!Storage_LoadConfig(localDoc)) {
      // Nếu lỗi load local config, ghi đè luôn
      Storage_SaveConfig(cloudDoc);
      AlarmManager_ReloadConfig();
      http.end();
      return;
    }

    JsonArray localComps = localDoc["compartments"].as<JsonArray>();
    bool configChanged = false;

    // So sánh cấu hình từng khay
    for (int i = 0; i < NUM_COMPARTMENTS; i++) {
      JsonObject lComp = localComps[i];
      
      // Tìm khay thuốc tương ứng bên Cloud theo ID
      JsonObject cComp;
      for (JsonObject cc : cloudComps) {
        if (cc["id"].as<int>() == lComp["id"].as<int>()) {
          cComp = cc;
          break;
        }
      }

      if (cComp.isNull()) continue;

      // So sánh các trường thông tin
      if (lComp["medicineName"].as<String>() != cComp["medicineName"].as<String>() ||
          lComp["scheduleTime"].as<String>() != cComp["scheduleTime"].as<String>() ||
          lComp["mealNote"].as<String>() != cComp["mealNote"].as<String>() ||
          lComp["enabled"].as<bool>() != cComp["enabled"].as<bool>()) {
        
        configChanged = true;
        break;
      }
    }

    if (configChanged) {
      DBGLN("[CLOUD] Phát hiện cấu hình Cloud thay đổi! Đang cập nhật cục bộ...");
      
      // Tạo tệp config mới đúng cấu trúc cục bộ để lưu
      DynamicJsonDocument newLocalDoc(2048);
      JsonArray newComps = newLocalDoc.createNestedArray("compartments");
      
      for (int i = 0; i < NUM_COMPARTMENTS; i++) {
        JsonObject cComp;
        for (JsonObject cc : cloudComps) {
          if (cc["id"].as<int>() == (i + 1)) {
            cComp = cc;
            break;
          }
        }
        
        JsonObject nComp = newComps.createNestedObject();
        nComp["id"] = i + 1;
        if (!cComp.isNull()) {
          nComp["medicineName"] = cComp["medicineName"];
          nComp["scheduleTime"] = cComp["scheduleTime"];
          nComp["mealNote"]     = cComp["mealNote"];
          nComp["enabled"]      = cComp["enabled"];
        } else {
          // Fallback giữ cấu hình cũ nếu cloud thiếu
          nComp["medicineName"] = localComps[i]["medicineName"];
          nComp["scheduleTime"] = localComps[i]["scheduleTime"];
          nComp["mealNote"]     = localComps[i]["mealNote"];
          nComp["enabled"]      = localComps[i]["enabled"];
        }
      }

      Storage_SaveConfig(newLocalDoc);
      AlarmManager_ReloadConfig();
      DBGLN("[CLOUD] Cấu hình 3 ngăn đã được cập nhật thành công.");
    }
  } else {
    DBG("[CLOUD] Lỗi kết nối kéo config (HTTP Code: ");
    DBG(httpCode);
    DBGLN(")");
  }

  http.end();
}
