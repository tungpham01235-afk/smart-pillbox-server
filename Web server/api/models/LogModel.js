'use strict';
const mongoose = require('mongoose');

const LogSchema = new mongoose.Schema({
    // Sử dụng boxId để truy vấn nhanh từ ESP32 và App
    boxId: { 
        type: String, 
        required: true, 
        index: true 
    }, 
    
    // Tên thuốc được tác động (Khớp với danh sách alarm)
    medicineName: { 
        type: String, 
        required: true 
    },
    
    // Trạng thái vận hành của hộp thuốc
    status: { 
        type: String, 
        enum: ['Đúng giờ', 'Quá giờ', 'Mở tủ sai', 'Đã uống', 'Chưa uống'], 
        required: true 
    },
    
    // Ghi chú chi tiết (Ví dụ: "Mạch ESP32 kích hoạt còi báo lúc 08:05")
    note: { 
        type: String, 
        default: '' 
    },
    
    // Thời gian ghi nhận bản ghi (Sử dụng trường này thay vì createdAt để đồng bộ chính xác thời gian thực)
    timeLog: { 
        type: Date, 
        default: Date.now,
        index: true
    }
});

// 🔥 BỔ SUNG TỐI ƯU: Compound Index (Chỉ mục hỗn hợp) cực kỳ quan trọng cho Cron Job!
// Giúp câu lệnh 'Log.findOne({ boxId, status: "Đã uống", timeLog: { $gte: ... } })' trong cronService
// chạy lập tức trong vài mili-giây, không quét toàn bộ database (Colscan), tránh treo Server khi dữ liệu lớn.
LogSchema.index({ boxId: 1, status: 1, timeLog: -1 });

module.exports = mongoose.model('Log', LogSchema);