/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — wifi_provisioning.h
 * Quản lý chế độ cấu hình Wi-Fi (AP Mode + Captive Portal)
 * ═══════════════════════════════════════════════════════════
 */

#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Khởi tạo tiến trình Wi-Fi (nếu chưa cấu hình hoặc cấu hình lỗi, tự động mở AP + Captive Portal)
void WifiProv_Init(AsyncWebServer &server);

// Hàm cập nhật chạy trong loop chính để duy trì DNS server (khi ở chế độ AP Mode)
void WifiProv_Loop();

// Kiểm tra xem thiết bị hiện tại có đang chạy ở chế độ AP phát cấu hình không
bool WifiProv_IsAPMode();

// Lệnh xóa cấu hình Wi-Fi cũ và khởi động lại thiết bị
void WifiProv_ForceReset();
