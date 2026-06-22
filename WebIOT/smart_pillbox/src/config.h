/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — config.h
 * Toan bo hang so cau hinh du an (KHONG co implementation)
 * ═══════════════════════════════════════════════════════════
 */

#pragma once
#include <Arduino.h>

// ════════════════════════════════════════
// A. Wi-Fi
// ════════════════════════════════════════

// ⚠️ THAY 2 DONG NAY TRUOC KHI NAP CODE
#define WIFI_SSID "Redmi Note 10 Pro"
#define WIFI_PASSWORD "0358748007"
#define WIFI_RETRY_COUNT 10
#define WIFI_RETRY_DELAY_MS 1000
#define WIFI_RECONNECT_INTERVAL_MS 30000

// ════════════════════════════════════════
// B. NTP & Thoi gian
// ════════════════════════════════════════

#define NTP_SERVER "asia.pool.ntp.org"
#define UTC_OFFSET_SEC 25200             // GMT+7 = 7 * 3600
#define NTP_UPDATE_INTERVAL_MS 3600000UL // Sync lai moi 1 gio

// ════════════════════════════════════════
// C. Cau hinh he thong
// ════════════════════════════════════════

#define NUM_COMPARTMENTS 3
#define MAX_LOG_ENTRIES 50  // So log toi da luu trong LittleFS
#define ALARM_WINDOW_MIN 30 // Cua so bao dong: 30 phut

// ════════════════════════════════════════
// D. GPIO Pins
// ════════════════════════════════════════

// Cam bien hong ngoai TCRT5000 (INPUT ONLY — GPIO 34, 35 khong co pull-up noi)
const int SENSOR_PINS[3] = {34, 35, 32};

// Den LED: Ngan 1=Do/GPIO25, Ngan 2=Vang/GPIO26, Ngan 3=Xanh/GPIO27
const int LED_PINS[3] = {25, 26, 27};

// Coi Buzzer thu dong (passive buzzer), active HIGH
#define BUZZER_PIN 14

// ════════════════════════════════════════
// E. Timing Non-blocking (millis)
// ════════════════════════════════════════

#define SENSOR_DEBOUNCE_MS 500         // Debounce cam bien
#define SCAN_INTERVAL_MS 200           // Tan suat vong lap quet
#define LED_BLINK_ON_MS 500            // LED sang 500ms
#define LED_BLINK_OFF_MS 500           // LED tat 500ms
#define BUZZER_BEEP_ON_MS 300          // Coi keu 300ms
#define BUZZER_BEEP_OFF_MS 700         // Coi nghi 700ms
#define TEST_ALARM_DURATION_MS 10000UL // Test alarm 10 giay

// ════════════════════════════════════════
// F. Ten nhan ngan (dung cho log va Web UI)
// ════════════════════════════════════════

static const char *COMPARTMENT_LABELS[3] = {
    "Ngan Sang", // Ngan 1 - Sang
    "Ngan Trua", // Ngan 2 - Trua
    "Ngan Toi"   // Ngan 3 - Toi
};

// Gia tri mac dinh khi tao config.json lan dau
static const char *DEFAULT_MEDICINE[3] = {"Thuoc da day", "Vitamin C",
                                          "Thuoc huyet ap"};
static const char *DEFAULT_SCHEDULE[3] = {"07:00", "12:00", "20:00"};
static const char *DEFAULT_MEAL_NOTE[3] = {"Sau khi an", "Sau khi an",
                                           "Truoc khi ngu"};

// ════════════════════════════════════════
// G. Debug macro
// ════════════════════════════════════════

#define DEBUG_ENABLED 1
#if DEBUG_ENABLED
#define DBG(x) Serial.print(x)
#define DBGLN(x) Serial.println(x)
#else
#define DBG(x)
#define DBGLN(x)
#endif

// ════════════════════════════════════════
// H. Cloud Sync Configuration
// ════════════════════════════════════════
#define CLOUD_SERVER_URL "https://smart-pillbox-server-production.up.railway.app" // URL của Web Server chạy trên Railway
#define CLOUD_DEVICE_KEY "SmartPillboxSecretKey2026"
#define CLOUD_BOX_ID "box_01"            // ID thiết bị này để khớp với DB
#define CLOUD_SYNC_INTERVAL_MS 2000UL   // Định kỳ kéo cấu hình mỗi 2s (kết nối keep-alive tối ưu)
#define CLOUD_LOG_RETRY_INTERVAL 60000UL // Định kỳ gửi bù logs offline mỗi 60s
