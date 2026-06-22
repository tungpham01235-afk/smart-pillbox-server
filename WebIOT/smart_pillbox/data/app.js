// ═══════════════════════════════════════════════════════════
// TỦ THUỐC THÔNG MINH — app.js
// Logic giao dien (Vanilla JS)
// ═══════════════════════════════════════════════════════════

// ════════════════════════════
// CONSTANTS
// ════════════════════════════
const STATUS_POLL_INTERVAL  = 1500;   // ms — poll /api/status moi 1.5s
const LOGS_POLL_INTERVAL    = 15000;  // ms — poll /api/logs moi 15s
const TOAST_DURATION        = 3000;   // ms — toast tu an sau 3s

// ════════════════════════════
// STATE
// ════════════════════════════
let currentTab = 'dashboard';
let statusInterval = null;
let logsInterval  = null;
let configLoaded  = false;

// ════════════════════════════
// DEMO MODE / SIMULATOR STATE
// ════════════════════════════
let isDemoMode = false;
let demoState = {
  currentTime: "--:--:--",
  ntpSynced: true,
  wifiRSSI: -45,
  compartments: [
    { id: 1, medicineName: "Thuốc dạ dày", scheduleTime: "07:00", mealNote: "Sau khi ăn", enabled: true, isMedicinePresent: true, alarmState: "IDLE" },
    { id: 2, medicineName: "Vitamin C", scheduleTime: "12:00", mealNote: "Sau khi ăn", enabled: true, isMedicinePresent: true, alarmState: "IDLE" },
    { id: 3, medicineName: "Thuốc huyết áp", scheduleTime: "20:00", mealNote: "Trước khi ngủ", enabled: true, isMedicinePresent: true, alarmState: "IDLE" }
  ],
  logs: []
};

// Khôi phục cấu hình / nhật ký demo từ localStorage nếu có
if (localStorage.getItem('pillbox_demo_config')) {
  try {
    const savedComps = JSON.parse(localStorage.getItem('pillbox_demo_config'));
    demoState.compartments = demoState.compartments.map(c => {
      const match = savedComps.find(x => x.id === c.id);
      return match ? { ...c, ...match } : c;
    });
  } catch (e) {
    console.error("Lỗi khôi phục cấu hình demo:", e);
  }
}

if (localStorage.getItem('pillbox_demo_logs')) {
  try {
    demoState.logs = JSON.parse(localStorage.getItem('pillbox_demo_logs'));
  } catch (e) {
    console.error("Lỗi khôi phục nhật ký demo:", e);
  }
}

// Giả lập thời gian chạy cữ báo thức tự động
function checkDemoAlarms(currentTime) {
  const [hh, mm, ss] = currentTime.split(':');
  const timeHHMM = `${hh}:${mm}`;
  
  demoState.compartments.forEach(comp => {
    if (!comp.enabled) return;
    
    // Nếu đến giờ và ở giây thứ 00, kích hoạt báo động
    if (timeHHMM === comp.scheduleTime && ss === '00' && comp.alarmState === 'IDLE' && comp.isMedicinePresent) {
      comp.alarmState = 'ALARMING';
      showToast(`🔔 Đến giờ hẹn: Báo thức ngăn ${comp.id} (${comp.medicineName})!`, 'info');
      
      // Cập nhật lại UI trong simulator
      const btnAlarm = document.getElementById(`sim-comp${comp.id}-alarm`);
      if (btnAlarm) btnAlarm.textContent = "🔔 Đang kêu...";
    }
  });
}

function addDemoLog(compartmentId, type, message) {
  const now = new Date();
  const dateStr = now.toLocaleDateString('vi-VN');
  const timeStr = now.toTimeString().split(' ')[0];
  const newLog = {
    ts: `${dateStr} ${timeStr}`,
    compartmentId: compartmentId,
    type: type, // 'ON_TIME', 'OFF_SCHEDULE', 'MISSED'
    message: message
  };
  demoState.logs.unshift(newLog);
  if (demoState.logs.length > 50) demoState.logs.pop();
  localStorage.setItem('pillbox_demo_logs', JSON.stringify(demoState.logs));
}

function enableDemoMode() {
  if (isDemoMode) return;
  isDemoMode = true;
  console.log("%c[DEMO MODE] Khởi động chế độ giả lập phần cứng offline!", "color: #2563eb; font-size: 14px; font-weight: bold;");
  
  // Hiển thị toast thông báo
  showToast("✨ Đã tự động bật Chế độ Giả lập (Không cần ESP32)!", "info");
  
  // Tạo bảng điều khiển giả lập phần cứng
  const panel = document.createElement('div');
  panel.id = 'sim-panel';
  panel.className = 'sim-panel';
  panel.innerHTML = `
    <div class="sim-header">
      <span>🛠️ GIẢ LẬP PHẦN CỨNG ESP32</span>
      <button class="sim-toggle-btn" id="sim-minimize-btn">−</button>
    </div>
    <div class="sim-body" id="sim-body">
      <div class="sim-comp-row">
        <strong>Ngăn 1 (Sáng)</strong>
        <label class="sim-switch">
          <input type="checkbox" id="sim-comp1-present">
          Có thuốc
        </label>
        <button class="sim-btn" id="sim-comp1-alarm">🔔 Báo thức</button>
      </div>
      <div class="sim-comp-row">
        <strong>Ngăn 2 (Trưa)</strong>
        <label class="sim-switch">
          <input type="checkbox" id="sim-comp2-present">
          Có thuốc
        </label>
        <button class="sim-btn" id="sim-comp2-alarm">🔔 Báo thức</button>
      </div>
      <div class="sim-comp-row">
        <strong>Ngăn 3 (Tối)</strong>
        <label class="sim-switch">
          <input type="checkbox" id="sim-comp3-present">
          Có thuốc
        </label>
        <button class="sim-btn" id="sim-comp3-alarm">🔔 Báo thức</button>
      </div>
      <div class="sim-info">
        <small>* <b>Hướng dẫn giả lập:</b><br>
        1. Bấm <b>Báo thức</b> để còi kêu/LED sáng (Card nhấp nháy đỏ).<br>
        2. Tắt check <b>Có thuốc</b> (tượng trưng cho lấy thuốc ra khỏi khay) để tắt báo thức và ghi nhận lịch sử uống thuốc.</small>
      </div>
    </div>
  `;
  document.body.appendChild(panel);
  
  // Nút thu nhỏ/mở rộng bảng điều khiển
  const minimizeBtn = document.getElementById('sim-minimize-btn');
  minimizeBtn.addEventListener('click', (e) => {
    e.stopPropagation();
    panel.classList.toggle('minimized');
    minimizeBtn.textContent = panel.classList.contains('minimized') ? '+' : '−';
  });
  
  // Thiết lập sự kiện cho từng khay trong panel giả lập
  [1, 2, 3].forEach(id => {
    const chk = document.getElementById(`sim-comp${id}-present`);
    const btnAlarm = document.getElementById(`sim-comp${id}-alarm`);
    const comp = demoState.compartments.find(c => c.id === id);
    
    // Đồng bộ trạng thái ban đầu của checkbox
    chk.checked = comp.isMedicinePresent;
    
    chk.addEventListener('change', (e) => {
      const present = e.target.checked;
      comp.isMedicinePresent = present;
      
      if (!present) {
        // Bệnh nhân rút thuốc ra khỏi khay
        if (comp.alarmState === 'ALARMING') {
          comp.alarmState = 'IDLE';
          btnAlarm.textContent = "🔔 Báo thức";
          showToast(`✅ Ngăn ${id}: Đã uống thuốc đúng giờ!`, 'success');
          addDemoLog(id, 'ON_TIME', `Bệnh nhân đã lấy thuốc "${comp.medicineName}" ở ngăn ${id} đúng giờ hẹn.`);
        } else if (comp.alarmState === 'MISSED') {
          comp.alarmState = 'IDLE';
          btnAlarm.textContent = "🔔 Báo thức";
          showToast(`⚠️ Ngăn ${id}: Đã uống thuốc trễ cữ!`, 'info');
          addDemoLog(id, 'OFF_SCHEDULE', `Bệnh nhân uống thuốc trễ cữ hẹn tại ngăn ${id}.`);
        } else {
          showToast(`📤 Ngăn ${id}: Đã lấy thuốc ra khỏi khay.`, 'info');
        }
      } else {
        // Đặt thuốc vào khay
        showToast(`📥 Ngăn ${id}: Đã đặt thuốc mới vào khay.`, 'info');
      }
      fetchStatus();
    });
    
    btnAlarm.addEventListener('click', () => {
      if (!comp.enabled) {
        showToast(`⚠ Ngăn ${id} đang bị tắt (vui lòng bật kích hoạt trong Cài đặt).`, 'error');
        return;
      }
      if (comp.alarmState === 'ALARMING') {
        // Tắt báo động giả lập thủ công nếu muốn
        comp.alarmState = 'IDLE';
        btnAlarm.textContent = "🔔 Báo thức";
        showToast(`🔕 Đã dừng báo động ngăn ${id}`, 'info');
      } else {
        comp.alarmState = 'ALARMING';
        btnAlarm.textContent = "🔕 Tắt chuông";
        showToast(`🔔 Còi kêu và LED khay ${id} đang nhấp nháy đỏ!`, 'info');
      }
      fetchStatus();
    });
  });
}

// Giả lập xử lý GET API
function handleDemoGet(url) {
  const now = new Date();
  demoState.currentTime = now.toTimeString().split(' ')[0];
  
  // Kiểm tra cữ giờ tự động
  checkDemoAlarms(demoState.currentTime);
  
  if (url === '/api/status') {
    return {
      currentTime: demoState.currentTime,
      ntpSynced: demoState.ntpSynced,
      wifiRSSI: demoState.wifiRSSI,
      compartments: demoState.compartments
    };
  }
  
  if (url === '/api/config') {
    return {
      compartments: demoState.compartments.map(c => ({
        id: c.id,
        medicineName: c.medicineName,
        scheduleTime: c.scheduleTime,
        mealNote: c.mealNote,
        enabled: c.enabled
      }))
    };
  }
  
  if (url === '/api/logs') {
    return {
      count: demoState.logs.length,
      logs: demoState.logs
    };
  }
  
  if (url.startsWith('/api/test/')) {
    const id = parseInt(url.split('/').pop(), 10);
    const comp = demoState.compartments.find(c => c.id === id);
    if (comp) {
      comp.alarmState = 'ALARMING';
      const btnAlarm = document.getElementById(`sim-comp${id}-alarm`);
      if (btnAlarm) btnAlarm.textContent = "🔕 Tắt chuông";
      return { success: true };
    }
  }
  return null;
}

// Giả lập xử lý POST API
function handleDemoPost(url, body) {
  if (url === '/api/config') {
    demoState.compartments = demoState.compartments.map(c => {
      const updated = body.compartments.find(x => x.id === c.id);
      return updated ? { ...c, ...updated } : c;
    });
    localStorage.setItem('pillbox_demo_config', JSON.stringify(demoState.compartments));
    return { success: true };
  }
  return null;
}

// Giả lập xử lý DELETE API
function handleDemoDelete(url) {
  if (url === '/api/logs') {
    demoState.logs = [];
    localStorage.removeItem('pillbox_demo_logs');
    return { success: true };
  }
  return null;
}

// ════════════════════════════
// FETCH HELPERS
// ════════════════════════════
async function apiGet(url) {
  if (isDemoMode) return handleDemoGet(url);
  try {
    const res = await fetch(url);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return await res.json();
  } catch (err) {
    console.warn(`[API] GET ${url} thất bại. Chuyển sang chế độ giả lập.`, err);
    enableDemoMode();
    return handleDemoGet(url);
  }
}

async function apiPost(url, body) {
  if (isDemoMode) return handleDemoPost(url, body);
  try {
    const res = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return await res.json();
  } catch (err) {
    console.warn(`[API] POST ${url} thất bại. Chuyển sang chế độ giả lập.`, err);
    enableDemoMode();
    return handleDemoPost(url, body);
  }
}

async function apiDelete(url) {
  if (isDemoMode) return handleDemoDelete(url);
  try {
    const res = await fetch(url, { method: 'DELETE' });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return await res.json();
  } catch (err) {
    console.warn(`[API] DELETE ${url} thất bại. Chuyển sang chế độ giả lập.`, err);
    enableDemoMode();
    return handleDemoDelete(url);
  }
}

// ════════════════════════════
// DASHBOARD LOGIC
// ════════════════════════════
async function fetchStatus() {
  const data = await apiGet('/api/status');
  if (!data) return;
  
  // Cap nhat dong ho
  const clockEl = document.getElementById('clock-display');
  if (clockEl) clockEl.textContent = data.currentTime;
  
  // Cap nhat NTP + WiFi status
  const ntpEl = document.getElementById('ntp-status');
  if (ntpEl) ntpEl.textContent = data.ntpSynced ? '✓ NTP đồng bộ' : '⚠ Chưa đồng bộ';
  
  const wifiEl = document.getElementById('wifi-rssi');
  if (wifiEl) wifiEl.textContent = `${data.wifiRSSI} dBm`;
  
  // Cap nhat 3 card ngan thuoc
  if (data.compartments) {
    data.compartments.forEach(comp => {
      updateCompartmentCard(comp);
    });
  }
}

function updateCompartmentCard(comp) {
  const card = document.querySelector(`.compartment-card[data-id="${comp.id}"]`);
  if (!card) return;
  
  // Ten thuoc + ghi chu
  card.querySelector('.medicine-name').textContent = comp.medicineName;
  card.querySelector('.meal-note').textContent = comp.mealNote;
  card.querySelector('.schedule-time').textContent = `Giờ hẹn: ${comp.scheduleTime}`;
  
  // Badge trang thai
  const badge = card.querySelector('.status-badge');
  badge.className = 'status-badge';  // Reset classes
  
  if (comp.alarmState === 'ALARMING') {
    badge.classList.add('badge-alarming');
    badge.textContent = '🔔 ĐẾN GIỜ UỐNG THUỐC!';
    card.classList.add('is-alarming');
  } else if (comp.alarmState === 'MISSED') {
    badge.classList.add('badge-missed');
    badge.textContent = '❌ Quá giờ — chưa uống!';
    card.classList.remove('is-alarming');
  } else if (comp.isMedicinePresent) {
    badge.classList.add('badge-present');
    badge.textContent = '✓ Thuốc trong ngăn';
    card.classList.remove('is-alarming');
  } else {
    badge.classList.add('badge-absent');
    badge.textContent = '📤 Đã lấy thuốc';
    card.classList.remove('is-alarming');
  }
}

async function testAlarm(id) {
  const result = await apiGet(`/api/test/${id}`);
  if (result && result.success) {
    showToast(`🔔 Đang test ngăn ${id}... (10 giây)`, 'info');
  } else {
    showToast(`❌ Lỗi test ngăn ${id}`, 'error');
  }
}

// ════════════════════════════
// CONFIG LOGIC
// ════════════════════════════
async function fetchConfig() {
  const data = await apiGet('/api/config');
  if (!data || configLoaded) return;
  
  // Dien form tu data.compartments
  if (data.compartments) {
    data.compartments.forEach(comp => {
      const prefix = `comp${comp.id}`;
      document.getElementById(`${prefix}-medicine`).value  = comp.medicineName;
      document.getElementById(`${prefix}-schedule`).value  = comp.scheduleTime;
      document.getElementById(`${prefix}-mealnote`).value  = comp.mealNote;
      document.getElementById(`${prefix}-enabled`).checked = comp.enabled;
    });
    configLoaded = true;
  }
}

async function saveConfig() {
  const btn = document.getElementById('save-config-btn');
  btn.disabled = true;
  btn.textContent = 'Đang lưu...';
  
  const compartments = [1, 2, 3].map(id => ({
    id: id,
    medicineName: document.getElementById(`comp${id}-medicine`).value,
    scheduleTime: document.getElementById(`comp${id}-schedule`).value,
    mealNote:     document.getElementById(`comp${id}-mealnote`).value,
    enabled:      document.getElementById(`comp${id}-enabled`).checked
  }));
  
  const result = await apiPost('/api/config', { compartments });
  
  btn.disabled = false;
  btn.textContent = '💾 Lưu Cài Đặt';
  
  if (result && result.success) {
    showToast('✅ Đã lưu cấu hình thành công!', 'success');
    configLoaded = false;  // Force reload neu vao lai tab
  } else {
    showToast('❌ Lỗi khi lưu cấu hình!', 'error');
  }
}

// ════════════════════════════
// LOGS LOGIC
// ════════════════════════════
async function fetchLogs() {
  const data = await apiGet('/api/logs');
  if (!data) return;
  
  const tbody = document.getElementById('logs-table-body');
  if (!tbody) return;
  
  tbody.innerHTML = '';  // Xoa cu
  
  if (data.count === 0 || !data.logs || data.logs.length === 0) {
    const row = document.createElement('tr');
    const td  = document.createElement('td');
    td.colSpan = 4;
    td.textContent = 'Chưa có hoạt động nào được ghi nhận.';
    td.className = 'empty-log';
    row.appendChild(td);
    tbody.appendChild(row);
    return;
  }
  
  data.logs.forEach(log => {
    const row = document.createElement('tr');
    row.className = `log-row log-${log.type.toLowerCase()}`;
    
    const typeLabel = {
      'ON_TIME':      '✅ Đúng cữ',
      'OFF_SCHEDULE': '⚠️ Sai cữ',
      'MISSED':       '❌ Bỏ cữ'
    }[log.type] || log.type;
    
    const columns = [
      { text: log.ts, label: 'Thời gian' },
      { text: `Ngăn ${log.compartmentId}`, label: 'Ngăn' },
      { text: typeLabel, label: 'Loại' },
      { text: log.message, label: 'Nội dung' }
    ];
    
    columns.forEach(col => {
      const td = document.createElement('td');
      td.textContent = col.text;
      td.setAttribute('data-label', col.label);
      row.appendChild(td);
    });
    
    tbody.appendChild(row);
  });
}

async function clearLogs() {
  if (!confirm('Bạn có chắc muốn xóa toàn bộ nhật ký không?\nHành động này không thể hoàn tác.')) return;
  
  const result = await apiDelete('/api/logs');
  if (result && result.success) {
    showToast('🗑️ Đã xóa nhật ký thành công', 'success');
    fetchLogs();
  } else {
    showToast('❌ Lỗi khi xóa nhật ký', 'error');
  }
}

// ════════════════════════════
// UI HELPERS
// ════════════════════════════
function showToast(message, type = 'success') {
  // Xoa toast cu neu co
  const existing = document.querySelector('.toast');
  if (existing) existing.remove();
  
  const toast = document.createElement('div');
  toast.className = `toast ${type}`;
  toast.textContent = message;
  document.body.appendChild(toast);
  
  setTimeout(() => {
    toast.classList.add('toast-out');
    setTimeout(() => toast.remove(), 300);
  }, TOAST_DURATION);
}

function switchTab(tabId) {
  // Update UI tabs
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.tab === tabId);
  });
  
  document.querySelectorAll('.tab-content').forEach(content => {
    content.classList.toggle('active', content.id === `tab-${tabId}`);
  });
  
  currentTab = tabId;
  
  // Handle specific tab logic
  if (tabId === 'config') {
    fetchConfig();
  } else if (tabId === 'logs') {
    fetchLogs();
    // Start polling logs
    if (!logsInterval) {
      logsInterval = setInterval(fetchLogs, LOGS_POLL_INTERVAL);
    }
  }
  
  // Stop polling logs neu roi khoi tab logs
  if (tabId !== 'logs' && logsInterval) {
    clearInterval(logsInterval);
    logsInterval = null;
  }
}

// ════════════════════════════
// INIT
// ════════════════════════════
document.addEventListener('DOMContentLoaded', () => {
  // Bind tab clicks
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => switchTab(btn.dataset.tab));
  });
  
  // Bind button actions
  const btnSaveConfig = document.getElementById('save-config-btn');
  if (btnSaveConfig) btnSaveConfig.addEventListener('click', saveConfig);
  
  const btnClearLogs = document.getElementById('clear-logs-btn');
  if (btnClearLogs) btnClearLogs.addEventListener('click', clearLogs);
  
  // Bind test alarm buttons
  document.querySelectorAll('.test-btn').forEach(btn => {
    btn.addEventListener('click', (e) => {
      const id = e.target.closest('.compartment-card').dataset.id;
      testAlarm(id);
    });
  });
  
  // Initial fetch
  fetchStatus();
  
  // Start dashboard polling
  statusInterval = setInterval(fetchStatus, STATUS_POLL_INTERVAL);
});
