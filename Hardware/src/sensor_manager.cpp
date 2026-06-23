/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — sensor_manager.cpp
 * Implementation: Doc cam bien IR voi debounce + edge detect
 *
 * Nguyen ly TCRT5000:
 *   - Hop thuoc HIEN DIEN → cam bien xuat muc LOW
 *   - Hop thuoc VANG MAT  → cam bien xuat muc HIGH
 *
 * GPIO 34, 35 la INPUT ONLY — KHONG co pull-up noi bo
 * ═══════════════════════════════════════════════════════════
 */

#include "sensor_manager.h"
#include "config.h"

// ════════════════════════════════════════
// Bien noi bo (static arrays)
// ════════════════════════════════════════
static SensorState   _currentState[NUM_COMPARTMENTS];
static SensorState   _rawState[NUM_COMPARTMENTS];
static unsigned long _lastRawChangeTime[NUM_COMPARTMENTS];
static bool          _justChangedToAbsent[NUM_COMPARTMENTS];
static bool          _justChangedToPresent[NUM_COMPARTMENTS];

// ════════════════════════════════════════
// SensorManager_Init — Khoi tao GPIO va doc trang thai ban dau
// ════════════════════════════════════════
void SensorManager_Init() {
  for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    // GPIO 34, 35 chi ho tro INPUT (khong co pull-up noi)
    pinMode(SENSOR_PINS[i], INPUT);

    // Doc trang thai ban dau
    SensorState initState = (digitalRead(SENSOR_PINS[i]) == LOW)
                            ? SENSOR_PRESENT : SENSOR_ABSENT;
    _currentState[i]      = initState;
    _rawState[i]          = initState;
    _lastRawChangeTime[i] = millis();

    // Reset tat ca flag edge detection
    _justChangedToAbsent[i]  = false;
    _justChangedToPresent[i] = false;

    DBG("[SENSOR] Ngan ");
    DBG(i + 1);
    DBG(": ");
    DBGLN(initState == SENSOR_PRESENT ? "PRESENT" : "ABSENT");
  }
  DBGLN("[SENSOR] Khoi tao 3 cam bien IR");
}

// ════════════════════════════════════════
// SensorManager_Update — Goi trong loop(), debounce + edge
// ════════════════════════════════════════
void SensorManager_Update() {
  for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    // 1. Doc raw
    SensorState rawNow = (digitalRead(SENSOR_PINS[i]) == LOW)
                         ? SENSOR_PRESENT : SENSOR_ABSENT;

    // 2. Neu raw thay doi → ghi nhan thoi diem
    if (rawNow != _rawState[i]) {
      _lastRawChangeTime[i] = millis();
      _rawState[i] = rawNow;
    }

    // 3. Kiem tra debounce: trang thai on dinh du lau
    if ((millis() - _lastRawChangeTime[i]) >= SENSOR_DEBOUNCE_MS) {
      // Trang thai on dinh — kiem tra co thay doi so voi confirmed khong
      if (rawNow != _currentState[i]) {
        // Edge detection
        if (rawNow == SENSOR_ABSENT) {
          _justChangedToAbsent[i] = true;
          DBG("[SENSOR] Ngan ");
          DBG(i + 1);
          DBGLN(": PRESENT -> ABSENT (da nhac hop)");
        } else {
          _justChangedToPresent[i] = true;
          DBG("[SENSOR] Ngan ");
          DBG(i + 1);
          DBGLN(": ABSENT -> PRESENT (da tra hop)");
        }

        // Cap nhat trang thai confirmed
        _currentState[i] = rawNow;
      }
    }
    // NOTE: KHONG reset flags o day — de caller doc duoc
  }
}

// ════════════════════════════════════════
// SensorManager_GetState — Trang thai hien tai
// ════════════════════════════════════════
SensorState SensorManager_GetState(int idx) {
  if (idx < 0 || idx >= NUM_COMPARTMENTS) return SENSOR_ABSENT;
  return _currentState[idx];
}

// ════════════════════════════════════════
// SensorManager_JustChangedToAbsent — Consume-once pattern
// Moi lan goi chi tra true 1 lan duy nhat
// ════════════════════════════════════════
bool SensorManager_JustChangedToAbsent(int idx) {
  if (idx < 0 || idx >= NUM_COMPARTMENTS) return false;
  bool val = _justChangedToAbsent[idx];
  _justChangedToAbsent[idx] = false;  // Reset sau khi doc
  return val;
}

// ════════════════════════════════════════
// SensorManager_JustChangedToPresent — Consume-once pattern
// ════════════════════════════════════════
bool SensorManager_JustChangedToPresent(int idx) {
  if (idx < 0 || idx >= NUM_COMPARTMENTS) return false;
  bool val = _justChangedToPresent[idx];
  _justChangedToPresent[idx] = false;  // Reset sau khi doc
  return val;
}
