/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — alarm_manager.h
 * State Machine bao dong: IDLE → ALARMING → ACKNOWLEDGED/MISSED
 *
 * 3 kich ban chinh:
 *   A: Den gio + thuoc trong ngan → LED + Buzzer (ALARMING)
 *   B: Dang ALARMING + nhac hop  → Tat alarm + log ON_TIME
 *   C: IDLE + nhac hop sai cu    → Chi log OFF_SCHEDULE
 * ═══════════════════════════════════════════════════════════
 */

#pragma once
#include <Arduino.h>
#include "config.h"

enum AlarmState {
  ALARM_IDLE,          // Binh thuong
  ALARM_ALARMING,      // Dang bao dong (LED + Buzzer)
  ALARM_ACKNOWLEDGED,  // Vua lay thuoc dung cu (trang thai chuyen tiep)
  ALARM_MISSED         // Qua gio khong lay thuoc
};

// Struct luu cau hinh 1 ngan (duoc load tu config.json)
struct CompartmentConfig {
  String medicineName;
  String scheduleTime;   // "HH:MM"
  String mealNote;
  bool   enabled;
};

void       AlarmManager_Init();
void       AlarmManager_Update();
AlarmState AlarmManager_GetState(int idx);
bool       AlarmManager_IsAnyAlarmActive();
void       AlarmManager_ReloadConfig();              // Reload sau khi Web luu config moi
void       AlarmManager_TriggerTest(int idx);        // Test alarm 10 giay
String     AlarmManager_GetConfigMedicineName(int idx);
String     AlarmManager_GetConfigScheduleTime(int idx);
String     AlarmManager_GetConfigMealNote(int idx);
bool       AlarmManager_GetConfigEnabled(int idx);
