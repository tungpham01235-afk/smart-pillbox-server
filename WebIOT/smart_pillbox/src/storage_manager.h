/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — storage_manager.h
 * Quan ly doc/ghi config va log tren LittleFS
 * ═══════════════════════════════════════════════════════════
 */

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

bool Storage_Init();
bool Storage_LoadConfig(JsonDocument& outDoc);
bool Storage_SaveConfig(const JsonDocument& inDoc);
bool Storage_AppendLog(int compartmentId, const String& type,
                       const String& message, const String& timestamp);
bool Storage_GetLogs(JsonDocument& outDoc);
bool Storage_ClearLogs();

// WiFi credentials storage
bool Storage_LoadWifi(String &ssid, String &pass, String &boxId, String &devKey);
bool Storage_SaveWifi(const String &ssid, const String &pass, const String &boxId, const String &devKey);
void Storage_ClearWifi();
