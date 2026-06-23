/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — time_manager.cpp
 * Implementation: NTP sync, time formatting, alarm window
 * ═══════════════════════════════════════════════════════════
 */

#include "time_manager.h"
#include "config.h"
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// ════════════════════════════════════════
// Bien noi bo (static)
// ════════════════════════════════════════
static WiFiUDP ntpUDP;
static NTPClient timeClient(ntpUDP, NTP_SERVER, UTC_OFFSET_SEC,
                            NTP_UPDATE_INTERVAL_MS);
static bool _timeValid = false;
static unsigned long _lastNtpUpdate = 0;

// ════════════════════════════════════════
// TimeManager_Init — Khoi tao va dong bo NTP
// ════════════════════════════════════════
bool TimeManager_Init() {
  timeClient.begin();

  // Thu dong bo toi da 5 lan
  DBGLN("[NTP] Dang dong bo thoi gian...");
  for (int attempt = 0; attempt < 5; attempt++) {
    if (timeClient.forceUpdate()) {
      _timeValid = true;
      DBG("[NTP] Dong bo thanh cong: ");
      DBGLN(TimeManager_GetTimeString());
      return true;
    }
    DBG(".");
    delay(2000); // Chap nhan delay trong Init (chi chay 1 lan)
  }

  _timeValid = false;
  DBGLN("\n[NTP] CANH BAO: Khong the dong bo gio!");
  return false;
}

// ════════════════════════════════════════
// TimeManager_Update — Goi trong loop (tu throttle)
// ════════════════════════════════════════
void TimeManager_Update() {
  timeClient.update();
  if (timeClient.isTimeSet()) {
    _timeValid = true;
  }
}

// ════════════════════════════════════════
// TimeManager_IsTimeValid
// ════════════════════════════════════════
bool TimeManager_IsTimeValid() { return _timeValid; }

// ════════════════════════════════════════
// TimeManager_GetHour / GetMinute
// ════════════════════════════════════════
int TimeManager_GetHour() { return timeClient.getHours(); }

int TimeManager_GetMinute() { return timeClient.getMinutes(); }

// ════════════════════════════════════════
// TimeManager_GetTimeString — "HH:MM:SS"
// ════════════════════════════════════════
String TimeManager_GetTimeString() {
  unsigned long epochTime = timeClient.getEpochTime();
  int hours = (epochTime % 86400UL) / 3600;
  int minutes = (epochTime % 3600) / 60;
  int seconds = epochTime % 60;

  char buf[12];
  sprintf(buf, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(buf);
}

// ════════════════════════════════════════
// TimeManager_GetHMString — "HH:MM"
// ════════════════════════════════════════
String TimeManager_GetHMString() {
  return TimeManager_GetTimeString().substring(0, 5);
}

// ════════════════════════════════════════
// TimeManager_GetDateTimeString — "YYYY-MM-DD HH:MM:SS"
// ════════════════════════════════════════
String TimeManager_GetDateTimeString() {
  unsigned long epochTime = timeClient.getEpochTime();
  time_t rawTime = (time_t)epochTime;
  struct tm *ti = gmtime(&rawTime);

  char buf[24];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", ti->tm_year + 1900,
          ti->tm_mon + 1, ti->tm_mday, ti->tm_hour, ti->tm_min, ti->tm_sec);
  return String(buf);
}

// ════════════════════════════════════════
// TimeManager_IsWithinAlarmWindow
// Kiem tra thoi gian hien tai co nam trong cua so bao dong
// ════════════════════════════════════════
bool TimeManager_IsWithinAlarmWindow(const String &scheduleTime,
                                     int windowMinutes) {
  if (!_timeValid)
    return false;

  // Parse scheduleTime "HH:MM"
  int colonIdx = scheduleTime.indexOf(':');
  if (colonIdx < 0)
    return false;

  int schedHour = scheduleTime.substring(0, colonIdx).toInt();
  int schedMin = scheduleTime.substring(colonIdx + 1).toInt();
  int schedTotalMin = schedHour * 60 + schedMin;
  int nowTotalMin = TimeManager_GetHour() * 60 + TimeManager_GetMinute();

  int windowEnd = schedTotalMin + windowMinutes;

  // Edge case: cua so qua nua dem
  // Vi du: scheduleTime = "23:45", window = 30 → windowEnd = 1455 (>= 1440)
  if (windowEnd >= 1440) {
    // Cua so tach thanh 2 phan:
    // Phan 1: schedTotalMin → 1439 (truoc nua dem)
    // Phan 2: 0 → (windowEnd - 1440) (sau nua dem)
    return (nowTotalMin >= schedTotalMin && nowTotalMin < 1440) ||
           (nowTotalMin >= 0 && nowTotalMin < (windowEnd - 1440));
  }

  // Truong hop binh thuong
  return (nowTotalMin >= schedTotalMin) && (nowTotalMin < windowEnd);
}

unsigned long TimeManager_GetAbsoluteDay() {
  if (!_timeValid) return 0;
  return timeClient.getEpochTime() / 86400UL;
}
