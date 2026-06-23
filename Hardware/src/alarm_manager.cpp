/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — alarm_manager.cpp
 * Implementation: State machine + LED blink + Buzzer beep
 * Tat ca non-blocking (dung millis(), KHONG delay())
 * ═══════════════════════════════════════════════════════════
 */

#include "alarm_manager.h"
#include "config.h"
#include "storage_manager.h"
#include "time_manager.h"
#include "sensor_manager.h"
#include "cloud_sync.h"
#include <ArduinoJson.h>

// ════════════════════════════════════════
// Bien noi bo (static)
// ════════════════════════════════════════

static AlarmState        _state[NUM_COMPARTMENTS];
static CompartmentConfig _config[NUM_COMPARTMENTS];
static unsigned long     _alarmStartTime[NUM_COMPARTMENTS];
static bool              _testMode[NUM_COMPARTMENTS];
static unsigned long     _testStartTime[NUM_COMPARTMENTS];
static unsigned long     _lastAlarmHandledDay[NUM_COMPARTMENTS];

// LED blink state (per compartment)
static bool              _ledOn[NUM_COMPARTMENTS];
static unsigned long     _ledLastToggle[NUM_COMPARTMENTS];

// Buzzer state (chung cho tat ca ngan)
static bool              _buzzerOn        = false;
static unsigned long     _buzzerLastToggle = 0;

// ════════════════════════════════════════
// Forward declarations (ham noi bo)
// ════════════════════════════════════════
static void _UpdateLEDs();
static void _UpdateBuzzer();

// ════════════════════════════════════════
// AlarmManager_Init
// ════════════════════════════════════════
void AlarmManager_Init() {
  // Cau hinh GPIO cho LED
  for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);

    _state[i]          = ALARM_IDLE;
    _testMode[i]       = false;
    _ledOn[i]          = false;
    _ledLastToggle[i]  = 0;
    _alarmStartTime[i] = 0;
    _testStartTime[i]  = 0;
    _lastAlarmHandledDay[i] = 0;
  }

  // Cau hinh GPIO cho Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Load config tu LittleFS
  AlarmManager_ReloadConfig();

  DBGLN("[ALARM] State machine san sang");
}

// ════════════════════════════════════════
// AlarmManager_ReloadConfig — Load tu config.json
// ════════════════════════════════════════
void AlarmManager_ReloadConfig() {
  DynamicJsonDocument doc(2048);

  if (!Storage_LoadConfig(doc)) {
    DBGLN("[ALARM] LOI: Khong the load config!");
    return;
  }

  JsonArray compartments = doc["compartments"].as<JsonArray>();
  for (int i = 0; i < NUM_COMPARTMENTS && i < (int)compartments.size(); i++) {
    JsonObject comp = compartments[i];
    String newMedName = comp["medicineName"].as<String>();
    String newSchedTime = comp["scheduleTime"].as<String>();
    String newMealNote = comp["mealNote"].as<String>();
    bool newEnabled = comp["enabled"] | true;

    // Reset cờ đã uống hôm nay nếu lịch uống hoặc thuốc có sự thay đổi
    if (_config[i].scheduleTime != newSchedTime || 
        _config[i].enabled != newEnabled || 
        _config[i].medicineName != newMedName) {
      _lastAlarmHandledDay[i] = 0;
      DBGLN("[ALARM] Lịch trình ngăn " + String(i + 1) + " thay đổi. Reset cờ hoàn thành.");
    }

    _config[i].medicineName = newMedName;
    _config[i].scheduleTime = newSchedTime;
    _config[i].mealNote     = newMealNote;
    _config[i].enabled      = newEnabled;
  }

  DBGLN("[ALARM] Config reloaded");
}

// ════════════════════════════════════════
// AlarmManager_Update — Goi trong loop()
// ════════════════════════════════════════
void AlarmManager_Update() {
  for (int i = 0; i < NUM_COMPARTMENTS; i++) {

    // === KIEM TRA TEST MODE ===
    if (_testMode[i]) {
      if (millis() - _testStartTime[i] > TEST_ALARM_DURATION_MS) {
        _testMode[i] = false;
        _state[i]    = ALARM_IDLE;
        DBG("[ALARM] Test ngan ");
        DBG(i + 1);
        DBGLN(" ket thuc");
      }
      continue;  // Trong test mode, bo qua logic chinh
    }

    // === XU LY EDGE DETECTION TU SENSOR ===
    bool justAbsent = SensorManager_JustChangedToAbsent(i);
    bool isPresent  = (SensorManager_GetState(i) == SENSOR_PRESENT);

    // KICH BAN B: Dang ALARMING, nguoi dung nhac hop len
    if (justAbsent && _state[i] == ALARM_ALARMING) {
      _state[i] = ALARM_IDLE;
      _lastAlarmHandledDay[i] = TimeManager_GetAbsoluteDay(); // Khóa báo động của cữ hôm nay

      String timeStr = TimeManager_GetTimeString();
      String msg = "[" + timeStr + "] " + _config[i].medicineName + ": Da uong DUNG GIO";
      Storage_AppendLog(i + 1, "ON_TIME", msg, TimeManager_GetDateTimeString());
      CloudSync_SendLog(i + 1, "Đúng giờ");

      DBG("[ALARM] Ngan ");
      DBG(i + 1);
      DBGLN(": Kich ban B - Dung cu!");
    }
    // KICH BAN C: Dang IDLE, nhac hop sai cu
    else if (justAbsent && _state[i] == ALARM_IDLE) {
      // KHONG hu coi, KHONG doi state
      String timeStr = TimeManager_GetTimeString();
      String msg = "[" + timeStr + "] " + _config[i].medicineName + ": Nhac hop ngoai cu";
      Storage_AppendLog(i + 1, "OFF_SCHEDULE", msg, TimeManager_GetDateTimeString());
      CloudSync_SendLog(i + 1, "Mở tủ sai");

      DBG("[ALARM] Ngan ");
      DBG(i + 1);
      DBGLN(": Kich ban C - Sai cu!");
    }

    // KIEM TRA QUA GIO (MISSED)
    if (_state[i] == ALARM_ALARMING) {
      unsigned long alarmDuration = (millis() - _alarmStartTime[i]) / 60000UL;
      if (alarmDuration >= ALARM_WINDOW_MIN) {
        _state[i] = ALARM_IDLE;
        _lastAlarmHandledDay[i] = TimeManager_GetAbsoluteDay(); // Khóa báo động cữ hôm nay do trễ hạn

        String timeStr = TimeManager_GetTimeString();
        String msg = "[" + timeStr + "] " + _config[i].medicineName + ": Qua gio - khong uong thuoc!";
        Storage_AppendLog(i + 1, "MISSED", msg, TimeManager_GetDateTimeString());
        CloudSync_SendLog(i + 1, "Bỏ lỡ");

        DBG("[ALARM] Ngan ");
        DBG(i + 1);
        DBGLN(": MISSED - Qua gio!");
      }
    }

    // KICH BAN A: Den gio hen + thuoc van trong ngan
    if (_config[i].enabled && _state[i] == ALARM_IDLE && isPresent) {
      if (TimeManager_IsWithinAlarmWindow(_config[i].scheduleTime)) {
        // Chỉ kích hoạt cữ báo động nếu ngày hôm nay chưa được xử lý
        if (_lastAlarmHandledDay[i] != TimeManager_GetAbsoluteDay()) {
          _state[i]          = ALARM_ALARMING;
          _alarmStartTime[i] = millis();

          DBG("[ALARM] Ngan ");
          DBG(i + 1);
          DBGLN(": Kich ban A - Den gio hen!");

          // Gửi thông báo đến giờ uống thuốc lên server ngay lập tức
          CloudSync_SendLog(i + 1, "Chưa uống");
        }
      }
    }
  }

  // === CAP NHAT LED VA BUZZER (NON-BLOCKING) ===
  _UpdateLEDs();
  _UpdateBuzzer();
}

// ════════════════════════════════════════
// _UpdateLEDs — Nhap nhay LED non-blocking
// ════════════════════════════════════════
static void _UpdateLEDs() {
  for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    if (_state[i] == ALARM_ALARMING || _testMode[i]) {
      // LED nhap nhay
      unsigned long interval = _ledOn[i] ? LED_BLINK_ON_MS : LED_BLINK_OFF_MS;
      if (millis() - _ledLastToggle[i] >= interval) {
        _ledOn[i] = !_ledOn[i];
        digitalWrite(LED_PINS[i], _ledOn[i] ? HIGH : LOW);
        _ledLastToggle[i] = millis();
      }
    } else {
      // Tat LED
      if (_ledOn[i]) {
        _ledOn[i] = false;
        digitalWrite(LED_PINS[i], LOW);
      }
    }
  }
}

// ════════════════════════════════════════
// _UpdateBuzzer — Beep non-blocking (chung)
// ════════════════════════════════════════
static void _UpdateBuzzer() {
  bool anyAlarm = AlarmManager_IsAnyAlarmActive();

  if (anyAlarm) {
    unsigned long interval = _buzzerOn ? BUZZER_BEEP_ON_MS : BUZZER_BEEP_OFF_MS;
    if (millis() - _buzzerLastToggle >= interval) {
      _buzzerOn = !_buzzerOn;
      digitalWrite(BUZZER_PIN, _buzzerOn ? HIGH : LOW);
      _buzzerLastToggle = millis();
    }
  } else {
    if (_buzzerOn) {
      _buzzerOn = false;
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}

// ════════════════════════════════════════
// AlarmManager_GetState
// ════════════════════════════════════════
AlarmState AlarmManager_GetState(int idx) {
  if (idx < 0 || idx >= NUM_COMPARTMENTS) return ALARM_IDLE;
  return _state[idx];
}

// ════════════════════════════════════════
// AlarmManager_IsAnyAlarmActive
// ════════════════════════════════════════
bool AlarmManager_IsAnyAlarmActive() {
  for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    if (_state[i] == ALARM_ALARMING || _testMode[i]) return true;
  }
  return false;
}

// ════════════════════════════════════════
// AlarmManager_TriggerTest — Test alarm 10 giay
// ════════════════════════════════════════
void AlarmManager_TriggerTest(int idx) {
  if (idx < 0 || idx >= NUM_COMPARTMENTS) return;

  _testMode[idx]      = true;
  _testStartTime[idx] = millis();
  _state[idx]         = ALARM_ALARMING;

  DBG("[ALARM] Test alarm ngan ");
  DBG(idx + 1);
  DBGLN(" trong 10 giay...");
}

// ════════════════════════════════════════
// Config Getters
// ════════════════════════════════════════
String AlarmManager_GetConfigMedicineName(int idx) {
  if (idx < 0 || idx >= NUM_COMPARTMENTS) return "";
  return _config[idx].medicineName;
}

String AlarmManager_GetConfigScheduleTime(int idx) {
  if (idx < 0 || idx >= NUM_COMPARTMENTS) return "";
  return _config[idx].scheduleTime;
}

String AlarmManager_GetConfigMealNote(int idx) {
  if (idx < 0 || idx >= NUM_COMPARTMENTS) return "";
  return _config[idx].mealNote;
}

bool AlarmManager_GetConfigEnabled(int idx) {
  if (idx < 0 || idx >= NUM_COMPARTMENTS) return false;
  return _config[idx].enabled;
}
