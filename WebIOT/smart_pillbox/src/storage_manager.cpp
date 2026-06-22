/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — storage_manager.cpp
 * Implementation: LittleFS init, config CRUD, log FIFO
 * ═══════════════════════════════════════════════════════════
 */

#include "storage_manager.h"
#include "config.h"
#include <LittleFS.h>

// ════════════════════════════════════════
// Storage_Init — Mount LittleFS + tao file mac dinh
// ════════════════════════════════════════
bool Storage_Init() {
  // Mount LittleFS (true = format neu mount loi lan dau)
  if (!LittleFS.begin(true)) {
    DBGLN("[STORAGE] LOI: Khong the mount LittleFS!");
    return false;
  }
  DBGLN("[STORAGE] LittleFS mounted OK");

  // Tao /config.json mac dinh neu chua ton tai
  if (!LittleFS.exists("/config.json")) {
    DBGLN("[STORAGE] Tao config.json mac dinh...");
    DynamicJsonDocument doc(2048);
    JsonArray compartments = doc.createNestedArray("compartments");

    for (int i = 0; i < NUM_COMPARTMENTS; i++) {
      JsonObject comp = compartments.createNestedObject();
      comp["id"]           = i + 1;
      comp["medicineName"] = DEFAULT_MEDICINE[i];
      comp["scheduleTime"] = DEFAULT_SCHEDULE[i];
      comp["mealNote"]     = DEFAULT_MEAL_NOTE[i];
      comp["enabled"]      = true;
    }

    File file = LittleFS.open("/config.json", "w");
    if (!file) {
      DBGLN("[STORAGE] Loi tao config.json!");
      return false;
    }
    serializeJson(doc, file);
    file.close();
    DBGLN("[STORAGE] config.json da tao thanh cong");
  }

  // Tao /logs.json mac dinh neu chua ton tai
  if (!LittleFS.exists("/logs.json")) {
    DBGLN("[STORAGE] Tao logs.json mac dinh...");
    File file = LittleFS.open("/logs.json", "w");
    if (!file) {
      DBGLN("[STORAGE] Loi tao logs.json!");
      return false;
    }
    file.print("[]");
    file.close();
    DBGLN("[STORAGE] logs.json da tao thanh cong");
  }

  return true;
}

// ════════════════════════════════════════
// Storage_LoadConfig — Doc /config.json
// ════════════════════════════════════════
bool Storage_LoadConfig(JsonDocument& outDoc) {
  File file = LittleFS.open("/config.json", "r");
  if (!file) {
    DBGLN("[STORAGE] Loi mo config.json de doc!");
    return false;
  }

  DynamicJsonDocument tempDoc(2048);
  DeserializationError err = deserializeJson(tempDoc, file);
  file.close();

  if (err) {
    DBG("[STORAGE] JSON parse loi config.json: ");
    DBGLN(err.c_str());
    return false;
  }

  // Copy sang outDoc
  outDoc.set(tempDoc);
  DBGLN("[STORAGE] config.json loaded OK");
  return true;
}

// ════════════════════════════════════════
// Storage_SaveConfig — Ghi de /config.json
// ════════════════════════════════════════
bool Storage_SaveConfig(const JsonDocument& inDoc) {
  File file = LittleFS.open("/config.json", "w");
  if (!file) {
    DBGLN("[STORAGE] Loi mo config.json de ghi!");
    return false;
  }

  serializeJson(inDoc, file);
  file.close();
  DBGLN("[STORAGE] config.json saved OK");
  return true;
}

// ════════════════════════════════════════
// Storage_AppendLog — Them log moi vao dau (FIFO)
// ════════════════════════════════════════
bool Storage_AppendLog(int compartmentId, const String& type,
                       const String& message, const String& timestamp) {
  // Doc logs hien tai
  DynamicJsonDocument doc(8192);
  
  File fileRead = LittleFS.open("/logs.json", "r");
  if (fileRead) {
    DeserializationError err = deserializeJson(doc, fileRead);
    fileRead.close();
    if (err) {
      DBGLN("[STORAGE] logs.json parse loi, reset ve array rong");
      doc.clear();
      doc.to<JsonArray>();
    }
  } else {
    doc.to<JsonArray>();
  }

  JsonArray logs = doc.as<JsonArray>();

  // Tao object log moi
  StaticJsonDocument<512> newEntry;
  newEntry["ts"]            = timestamp;
  newEntry["compartmentId"] = compartmentId;
  newEntry["type"]          = type;
  newEntry["message"]       = message;

  // Chen vao dau array (muc moi nhat o tren cung)
  // Cach: tao array moi, them entry moi truoc, roi copy cac entry cu
  DynamicJsonDocument newDoc(8192);
  JsonArray newLogs = newDoc.to<JsonArray>();
  newLogs.add(newEntry);

  for (JsonVariant v : logs) {
    if (newLogs.size() >= MAX_LOG_ENTRIES) break;
    newLogs.add(v);
  }

  // Ghi lai xuong file
  File fileWrite = LittleFS.open("/logs.json", "w");
  if (!fileWrite) {
    DBGLN("[STORAGE] Loi mo logs.json de ghi!");
    return false;
  }
  serializeJson(newDoc, fileWrite);
  fileWrite.close();

  DBG("[LOG] ");
  DBG(type);
  DBG(" - ");
  DBGLN(message);
  return true;
}

// ════════════════════════════════════════
// Storage_GetLogs — Doc logs, wrap ket qua
// ════════════════════════════════════════
bool Storage_GetLogs(JsonDocument& outDoc) {
  DynamicJsonDocument tempDoc(8192);

  File file = LittleFS.open("/logs.json", "r");
  if (!file) {
    DBGLN("[STORAGE] Loi mo logs.json de doc!");
    outDoc["count"] = 0;
    outDoc.createNestedArray("logs");
    return false;
  }

  DeserializationError err = deserializeJson(tempDoc, file);
  file.close();

  if (err) {
    DBGLN("[STORAGE] logs.json parse loi!");
    outDoc["count"] = 0;
    outDoc.createNestedArray("logs");
    return false;
  }

  JsonArray logs = tempDoc.as<JsonArray>();
  outDoc["count"] = logs.size();
  JsonArray outLogs = outDoc.createNestedArray("logs");
  for (JsonVariant v : logs) {
    outLogs.add(v);
  }

  return true;
}

// ════════════════════════════════════════
// Storage_ClearLogs — Xoa toan bo log
// ════════════════════════════════════════
bool Storage_ClearLogs() {
  File file = LittleFS.open("/logs.json", "w");
  if (!file) {
    DBGLN("[STORAGE] Loi mo logs.json de xoa!");
    return false;
  }
  file.print("[]");
  file.close();
  DBGLN("[STORAGE] Logs cleared OK");
  return true;
}
