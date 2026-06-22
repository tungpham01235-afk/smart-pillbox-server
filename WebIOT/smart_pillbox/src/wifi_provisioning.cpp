/**
 * ═══════════════════════════════════════════════════════════
 * TỦ THUỐC THÔNG MINH — wifi_provisioning.cpp
 * Triển khai chế độ cấu hình Wi-Fi thương mại (Captive Portal)
 * ═══════════════════════════════════════════════════════════
 */

#include "wifi_provisioning.h"
#include "storage_manager.h"
#include "config.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

static DNSServer dnsServer;
static bool isAPMode = false;
static bool shouldRestart = false;
static unsigned long restartTimer = 0;

// Giao diện web cấu hình Wi-Fi Glassmorphism (HTML/CSS/JS)
const char CONFIG_PORTAL_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Pillbox — Cấu Hình Wi-Fi</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;700&display=swap" rel="stylesheet">
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Outfit', sans-serif; }
        body {
            background: linear-gradient(135deg, #0f172a 0%, #1e1b4b 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
            color: #f1f5f9;
        }
        .container {
            width: 100%;
            max-width: 420px;
            background: rgba(30, 41, 59, 0.75);
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            border: 1px solid rgba(255, 255, 255, 0.1);
            border-radius: 24px;
            padding: 35px 30px;
            box-shadow: 0 20px 40px rgba(0,0,0,0.4);
            text-align: center;
        }
        .logo {
            font-size: 3rem;
            margin-bottom: 10px;
            display: inline-block;
        }
        h2 { font-size: 1.8rem; font-weight: 700; margin-bottom: 8px; color: #38bdf8; }
        .subtitle { font-size: 0.9rem; color: #94a3b8; margin-bottom: 30px; line-height: 1.4; }
        .form-group { text-align: left; margin-bottom: 20px; }
        label { display: block; font-size: 0.9rem; font-weight: 600; margin-bottom: 8px; color: #cbd5e1; }
        input, select {
            width: 100%;
            padding: 14px 16px;
            background: rgba(15, 23, 42, 0.6);
            border: 1px solid rgba(255,255,255,0.15);
            border-radius: 12px;
            color: #fff;
            font-size: 1rem;
            outline: none;
            transition: all 0.3s ease;
        }
        input:focus, select:focus {
            border-color: #38bdf8;
            box-shadow: 0 0 0 3px rgba(56, 189, 248, 0.2);
            background: rgba(15, 23, 42, 0.8);
        }
        select option { background: #0f172a; color: #fff; }
        .btn-submit {
            width: 100%;
            padding: 16px;
            background: linear-gradient(90deg, #0ea5e9 0%, #2563eb 100%);
            border: none;
            border-radius: 12px;
            color: #fff;
            font-size: 1.1rem;
            font-weight: 700;
            cursor: pointer;
            box-shadow: 0 8px 16px rgba(37, 99, 235, 0.3);
            transition: all 0.3s ease;
            margin-top: 10px;
        }
        .btn-submit:hover {
            transform: translateY(-2px);
            box-shadow: 0 12px 20px rgba(37, 99, 235, 0.4);
        }
        .btn-submit:active { transform: translateY(0); }
        .scan-container { display: flex; gap: 10px; }
        .btn-scan {
            padding: 0 16px;
            background: rgba(255, 255, 255, 0.08);
            border: 1px solid rgba(255,255,255,0.1);
            border-radius: 12px;
            color: #fff;
            cursor: pointer;
            font-size: 1.2rem;
            transition: all 0.3s;
        }
        .btn-scan:hover { background: rgba(255, 255, 255, 0.15); }
        .footer { font-size: 0.8rem; color: #64748b; margin-top: 30px; }
        .success-icon { font-size: 4rem; color: #22c55e; margin-bottom: 20px; }
        .success-title { font-size: 1.6rem; color: #22c55e; margin-bottom: 10px; }
        .loader {
            border: 3px solid rgba(255,255,255,0.1);
            border-top: 3px solid #38bdf8;
            border-radius: 50%;
            width: 20px;
            height: 20px;
            animation: spin 1s linear infinite;
            display: inline-block;
            vertical-align: middle;
            margin-right: 8px;
        }
        @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
    </style>
</head>
<body>
    <div class="container" id="main-card">
        <span class="logo">💊</span>
        <h2>Smart Pillbox</h2>
        <p class="subtitle">Cấu hình kết nối Wi-Fi & Máy chủ cho thiết bị nhắc thuốc thông minh.</p>
        
        <form id="wifi-form" action="/save-wifi" method="POST" onsubmit="handleSubmit(event)">
            <div class="form-group">
                <label>Tên Wi-Fi (SSID)</label>
                <div class="scan-container">
                    <select id="ssid-select" name="ssid" required>
                        <option value="">Đang quét Wi-Fi...</option>
                    </select>
                    <button type="button" class="btn-scan" onclick="scanWifi()">🔄</button>
                </div>
            </div>
            
            <div class="form-group">
                <label>Mật khẩu Wi-Fi</label>
                <input type="password" name="password" placeholder="Nhập mật khẩu..." required>
            </div>

            <div class="form-group">
                <label>Mã số tủ (Box ID)</label>
                <input type="text" name="boxId" id="boxId" placeholder="Ví dụ: box_01" required>
            </div>

            <div class="form-group">
                <label>Mã khóa thiết bị (Secret Key)</label>
                <input type="text" name="devKey" id="devKey" required>
            </div>
            
            <button type="submit" class="btn-submit" id="submit-btn">Lưu cấu hình</button>
        </form>
        <p class="footer">Smart Pillbox v1.0 — Powered by ESP32</p>
    </div>

    <div class="container" id="success-card" style="display:none;">
        <div class="success-icon">✓</div>
        <div class="success-title">Đã Lưu Thiết Lập!</div>
        <p class="subtitle" style="margin-bottom: 20px;">Tủ thuốc đang khởi động lại để kết nối với mạng Wi-Fi mới của bạn.</p>
        <p class="subtitle" style="font-size:0.85rem; color:#38bdf8;">Hãy tắt Wi-Fi này và mở App/Web Server để kiểm tra kết nối Online sau 15-20 giây.</p>
    </div>

    <script>
        async function loadDefaults() {
            try {
                const res = await fetch('/api/defaults');
                if (res.ok) {
                    const data = await res.json();
                    if (data.boxId) document.getElementById('boxId').value = data.boxId;
                    if (data.devKey) document.getElementById('devKey').value = data.devKey;
                }
            } catch(e) {}
        }

        async function scanWifi() {
            const select = document.getElementById('ssid-select');
            if (select.value === "") {
                select.innerHTML = '<option value="">Đang quét mạng Wi-Fi...</option>';
            }
            try {
                const res = await fetch('/api/scan-wifi');
                if (res.ok) {
                    const data = await res.json();
                    
                    if (data.status === "scanning") {
                        // Vẫn đang quét, thử lại sau 1.5 giây
                        setTimeout(scanWifi, 1500);
                        return;
                    }

                    select.innerHTML = '';
                    if (!Array.isArray(data) || data.length === 0) {
                        select.innerHTML = '<option value="">Không quét thấy Wi-Fi</option>';
                        return;
                    }
                    data.forEach(net => {
                        const opt = document.createElement('option');
                        opt.value = net.ssid;
                        opt.textContent = net.ssid + ' (' + net.rssi + ' dBm)';
                        select.appendChild(opt);
                    });
                }
            } catch(e) {
                select.innerHTML = '<option value="">Lỗi tải quét mạng Wi-Fi</option>';
            }
        }

        async function handleSubmit(e) {
            e.preventDefault();
            const btn = document.getElementById('submit-btn');
            btn.disabled = true;
            btn.innerHTML = '<span class="loader"></span>Đang lưu thiết lập...';

            const form = document.getElementById('wifi-form');
            const formData = new FormData(form);
            const params = new URLSearchParams(formData);

            try {
                const res = await fetch('/save-wifi?' + params.toString(), {
                    method: 'POST'
                });

                if (res.ok) {
                    document.getElementById('main-card').style.display = 'none';
                    document.getElementById('success-card').style.display = 'block';
                } else {
                    alert('Lỗi: Gặp sự cố khi lưu cấu hình.');
                    btn.disabled = false;
                    btn.textContent = 'Lưu cấu hình';
                }
            } catch (e) {
                alert('Lỗi kết nối tới thiết bị!');
                btn.disabled = false;
                btn.textContent = 'Lưu cấu hình';
            }
        }

        window.onload = async () => {
            await loadDefaults();
            await scanWifi();
        };
    </script>
</body>
</html>
)rawhtml";

void WifiProv_Init(AsyncWebServer &server) {
  String ssid = "";
  String pass = "";
  String boxId = "";
  String devKey = "";

  bool hasSavedWifi = Storage_LoadWifi(ssid, pass, boxId, devKey);

  if (hasSavedWifi) {
    DBGLN("[WIFI] Tim thay cau hinh cu. Dang thu ket noi STA...");
    
    // Ghi đè cấu hình Box ID và Device Key hoạt động nếu tìm thấy
    // (Vì các hằng số này được nạp tĩnh, cấu hình lưu sẽ được ưu tiên)
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 30) { // Chờ tối đa 15s (30 * 500ms)
      delay(500);
      DBG(".");
      retryCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      DBGLN("[WIFI] Kết nối Wi-Fi thành công!");
      DBG("[WIFI] IP thiết bị: ");
      DBGLN(WiFi.localIP().toString());
      return;
    } else {
      Serial.println();
      DBGLN("[WIFI] Không thể kết nối Wi-Fi đã lưu (Timeout 15s)!");
    }
  } else {
    DBGLN("[WIFI] Chưa có cấu hình Wi-Fi nào được lưu.");
  }

  // Khởi động chế độ cấu hình AP Mode + Captive Portal
  isAPMode = true;
  WiFi.mode(WIFI_AP_STA);
  
  // Phát mạng Wi-Fi không mật khẩu
  WiFi.softAP("Smart-Pillbox-Setup");
  IPAddress apIP = WiFi.softAPIP();
  
  DBGLN("[WIFI] Đã kích hoạt điểm phát Wi-Fi: Smart-Pillbox-Setup");
  DBG("[WIFI] IP Cấu hình: ");
  DBGLN(apIP.toString());

  // Khởi động lượt quét Wi-Fi bất đồng bộ đầu tiên
  WiFi.scanNetworks(true);

  // DNS Server chuyển hướng tất cả truy cập (*) về IP AP của ESP32
  dnsServer.start(53, "*", apIP);
  DBGLN("[WIFI] DNS Captive Portal Server bắt đầu.");

  // Đăng ký các endpoints cho trang cấu hình
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", CONFIG_PORTAL_HTML);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    request->send(response);
  });

  server.on("/api/scan-wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
    int16_t n = WiFi.scanComplete();
    
    if (n == WIFI_SCAN_RUNNING) {
      request->send(200, "application/json", "{\"status\":\"scanning\"}");
    } else if (n == WIFI_SCAN_FAILED) {
      // Quét lỗi hoặc chưa quét, kích hoạt quét lại bất đồng bộ
      WiFi.scanNetworks(true);
      request->send(200, "application/json", "{\"status\":\"scanning\"}");
    } else if (n >= 0) {
      DynamicJsonDocument doc(1024);
      JsonArray arr = doc.to<JsonArray>();
      
      // Thu thập danh sách Wi-Fi duy nhất (Unique SSIDs)
      std::vector<String> uniqueSSIDs;
      for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        if (ssid.length() > 0 && std::find(uniqueSSIDs.begin(), uniqueSSIDs.end(), ssid) == uniqueSSIDs.end()) {
          uniqueSSIDs.push_back(ssid);
          JsonObject obj = arr.createNestedObject();
          obj["ssid"] = ssid;
          obj["rssi"] = rssi;
        }
      }
      
      String json;
      serializeJson(doc, json);
      WiFi.scanDelete(); // Giải phóng bộ nhớ kết quả quét cũ
      
      // Kích hoạt luôn lượt quét mới ngầm để chuẩn bị cho lần quét sau
      WiFi.scanNetworks(true);
      
      request->send(200, "application/json", json);
    } else {
      WiFi.scanNetworks(true);
      request->send(200, "application/json", "{\"status\":\"scanning\"}");
    }
  });

  server.on("/api/defaults", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Đọc Box ID và Device Key hiện đang lưu hoặc mặc định
    String ssidVal, passVal, boxIdVal, devKeyVal;
    Storage_LoadWifi(ssidVal, passVal, boxIdVal, devKeyVal);
    
    DynamicJsonDocument doc(256);
    doc["boxId"] = boxIdVal.length() > 0 ? boxIdVal : String(CLOUD_BOX_ID);
    doc["devKey"] = devKeyVal.length() > 0 ? devKeyVal : String(CLOUD_DEVICE_KEY);
    
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/save-wifi", HTTP_ANY, [](AsyncWebServerRequest *request) {
    String reqSsid = "";
    String reqPass = "";
    String reqBoxId = "";
    String reqDevKey = "";

    if (request->hasParam("ssid")) reqSsid = request->getParam("ssid")->value();
    if (request->hasParam("password")) reqPass = request->getParam("password")->value();
    if (request->hasParam("boxId")) reqBoxId = request->getParam("boxId")->value();
    if (request->hasParam("devKey")) reqDevKey = request->getParam("devKey")->value();

    if (reqSsid.length() > 0) {
      Storage_SaveWifi(reqSsid, reqPass, reqBoxId, reqDevKey);
      request->send(200, "text/plain", "OK");

      // Đặt lệnh khởi động lại sau 2 giây
      shouldRestart = true;
      restartTimer = millis();
    } else {
      request->send(400, "text/plain", "SSID must not be empty");
    }
  });

  // Tự động chuyển hướng toàn bộ các request không đúng đường dẫn về trang chủ Captive Portal
  server.onNotFound([](AsyncWebServerRequest *request) {
    // Trả về 302 Found kèm các Header chống cache và đóng kết nối nhanh để kích hoạt Windows/MacOS Captive Portal
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
    response->addHeader("Location", "http://192.168.4.1/");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    response->addHeader("Connection", "close");
    request->send(response);
  });

  // Chạy Web Server
  server.begin();
  DBGLN("[WIFI] Web Server cấu hình khởi động thành công.");
}

void WifiProv_Loop() {
  if (isAPMode) {
    dnsServer.processNextRequest();
  }

  // Khởi động lại sau khi lưu cấu hình
  if (shouldRestart && (millis() - restartTimer >= 2000)) {
    DBGLN("[WIFI] Đang khởi động lại thiết bị...");
    ESP.restart();
  }
}

bool WifiProv_IsAPMode() {
  return isAPMode;
}

void WifiProv_ForceReset() {
  DBGLN("[WIFI] NHẬN LỆNH RESET: Xoá Wi-Fi cũ và khởi động lại...");
  Storage_ClearWifi();
  
  // Phát tín hiệu Bíp dài 1.5s cảnh báo
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1500);
  digitalWrite(BUZZER_PIN, LOW);
  
  ESP.restart();
}
