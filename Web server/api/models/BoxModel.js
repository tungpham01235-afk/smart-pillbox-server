'use strict';
const mongoose = require('mongoose');

// 1. Định nghĩa cấu trúc ngăn thuốc (Compartment)
const CompartmentSchema = new mongoose.Schema({
    id: { type: Number, required: true },          // 1: Sáng, 2: Trưa, 3: Tối
    medicineName: { type: String, default: '' },   // Tên thuốc
    scheduleTime: { type: String, default: '' },   // Giờ báo thức (Ví dụ: "07:00")
    mealNote: { 
        type: String, 
        default: 'Sau ăn' 
    },
    enabled: { type: Boolean, default: false },     // Trạng thái bật/tắt của ngăn
    isMedicinePresent: { type: Boolean, default: false } // Trạng thái cảm biến: Có thuốc/Trống
});

// 2. Định nghĩa cấu trúc của Hộp thuốc thông minh (Box)
const BoxSchema = new mongoose.Schema({
    boxId: { type: String, required: true, unique: true },
    ownerId: { type: mongoose.Schema.Types.ObjectId, ref: 'User', required: true },
    lastActive: { type: Date, default: null }, // Thời gian tương tác cuối của ESP32
    triggerTestAlarm: { type: Number, default: 0 }, // Cờ kích hoạt test còi/đèn vật lý (1-3)
    
    // Mảng chứa đúng 3 ngăn thuốc cố định
    compartments: { type: [CompartmentSchema], default: [] } 
}, { timestamps: true }); // Tự động thêm ngày tạo (createdAt) và cập nhật (updatedAt)

module.exports = mongoose.model('Box', BoxSchema);