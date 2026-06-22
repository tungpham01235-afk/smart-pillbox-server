'use strict';

module.exports = function(app) {
    // Import Controllers
    const authCtrl = require('./controllers/AuthController');
    const boxCtrl = require('./controllers/BoxController');
    
    // Import Middlewares (Nằm cùng thư mục api/)
    const checkAuth = require('./authMiddleware'); 
    const checkDevice = require('./deviceMiddleware');

    // =========================================================================
    // NHÓM 1: XÁC THỰC NGƯỜI DÙNG & HỒ SƠ (USER APPLICATION)
    // =========================================================================
    app.route('/api/auth/register').post(authCtrl.register);
    app.route('/api/auth/verifyOTP').post(authCtrl.verifyOTP);
    app.route('/api/auth/resendOTP').post(authCtrl.resendOTP);
    app.route('/api/auth/login').post(authCtrl.login);

    // Quản lý Hồ sơ (Yêu cầu đăng nhập User)
    app.route('/api/auth/profile')
        .get(checkAuth, authCtrl.getProfile)
        .put(checkAuth, authCtrl.updateProfile);
    
    app.route('/api/auth/profile/request-email-change').post(checkAuth, authCtrl.requestEmailChange);
    app.route('/api/auth/profile/verify-email-change').post(checkAuth, authCtrl.verifyEmailChange);

    // =========================================================================
    // NHÓM 2: QUẢN LÝ THIẾT BỊ & LỊCH TRÌNH (BOX & ALARMS)
    // =========================================================================
    // Người dùng quét QR kích hoạt / nhận hộp thuốc
    app.route('/api/box/bind').post(checkAuth, boxCtrl.bindBox); 
    
    // Đọc cấu hình và Cập nhật lịch nhắc thuốc từ giao diện App/Web Dashboard
    app.route('/api/box/:boxId')
        .get(checkAuth, boxCtrl.getBoxConfig)
        .put(checkAuth, boxCtrl.updateAlarms);
    
    // Giả lập báo động từ giao diện App/Web Dashboard
    app.route('/api/box/:boxId/test-alarm')
        .post(checkAuth, boxCtrl.testAlarm);

    // Đặt lại Wi-Fi thiết bị từ xa
    app.route('/api/box/:boxId/reset-wifi')
        .post(checkAuth, boxCtrl.resetDeviceWifi);

    // =========================================================================
    // NHÓM 3: ĐỒNG BỘ DỮ LIỆU & LỊCH SỬ (LOGS SYSTEM)
    // =========================================================================
    // Xem lịch sử uống thuốc trên App (Yêu cầu đăng nhập User)
    app.route('/api/logs/:boxId').get(checkAuth, boxCtrl.getBoxLogs);
    
    // Thiết bị phần cứng (ESP32) đẩy log thời gian thực lên hệ thống
    // Sử dụng checkDevice (Mã hóa/Token phần cứng riêng, không dùng tài khoản user)
    app.route('/api/logs/sync').post(checkDevice, boxCtrl.savePillLog); 
    app.route('/api/device/config').get(checkDevice, boxCtrl.getDeviceConfig);
};