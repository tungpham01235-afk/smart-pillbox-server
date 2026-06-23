/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — cloud_sync.h
 * Khai báo các hàm đồng bộ hóa cấu hình và gửi log lên Cloud
 * ═══════════════════════════════════════════════════════════
 */

#pragma once
#include <Arduino.h>

// Khởi tạo các cờ và biến đồng bộ
void CloudSync_Init();

// Gửi log uống thuốc lên Cloud Server
// Trả về true nếu gửi thành công, false nếu thất bại (sẽ lưu log offline để gửi bù)
bool CloudSync_SendLog(int compartmentId, const String& status);

// Định kỳ kéo cấu hình 3 ngăn từ Cloud Server (phi chặn)
void CloudSync_PullConfig(bool force = false);

// Gửi bù các logs bị lỗi khi mất Wi-Fi (gọi định kỳ)
void CloudSync_ProcessRetryLogs();
