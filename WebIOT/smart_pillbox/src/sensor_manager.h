/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — sensor_manager.h
 * Quan ly 3 cam bien hong ngoai TCRT5000 voi debounce
 * ═══════════════════════════════════════════════════════════
 */

#pragma once
#include <Arduino.h>
#include "config.h"

enum SensorState {
  SENSOR_PRESENT,   // Hop thuoc dang trong ngan (muc LOW)
  SENSOR_ABSENT     // Hop thuoc da duoc nhac ra (muc HIGH)
};

void        SensorManager_Init();
void        SensorManager_Update();
SensorState SensorManager_GetState(int idx);
bool        SensorManager_JustChangedToAbsent(int idx);   // Edge: PRESENT → ABSENT
bool        SensorManager_JustChangedToPresent(int idx);  // Edge: ABSENT → PRESENT
