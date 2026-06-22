'use strict';

module.exports = function(req, res, next) {
    const deviceKey = req.headers['x-device-key'];

    console.log('--- KIỂM TRA XÁC THỰC THIẾT BỊ ---');
    console.log('Nhận từ Header x-device-key:', deviceKey);
    console.log('Biến môi trường DEVICE_SECRET_KEY:', process.env.DEVICE_SECRET_KEY);

    const headerKeyClean = (deviceKey || '').trim();
    const envKeyClean = (process.env.DEVICE_SECRET_KEY || '').trim();

    // Kiểm tra key so với biến môi trường sau khi đã xóa khoảng trắng thừa
    if (headerKeyClean && envKeyClean && headerKeyClean === envKeyClean) {
        next();
    } else {
        // Sử dụng cú pháp chuẩn của Express: status() + json()
        return res.status(403).json({ 
            success: false,
            message: "Thiết bị không được xác thực!" 
        });
    }
};