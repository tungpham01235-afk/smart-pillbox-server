/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — time_manager.h
 * Quan ly dong bo thoi gian NTP va cac ham tien ich thoi gian
 * ═══════════════════════════════════════════════════════════
 */

#pragma once
#include <Arduino.h>
#include "config.h"

bool   TimeManager_Init();
void   TimeManager_Update();
bool   TimeManager_IsTimeValid();
String TimeManager_GetTimeString();        // "HH:MM:SS"
String TimeManager_GetHMString();          // "HH:MM"
String TimeManager_GetDateTimeString();    // "YYYY-MM-DD HH:MM:SS"
int    TimeManager_GetHour();
int    TimeManager_GetMinute();
bool   TimeManager_IsWithinAlarmWindow(const String& scheduleTime,
                                       int windowMinutes = ALARM_WINDOW_MIN);
