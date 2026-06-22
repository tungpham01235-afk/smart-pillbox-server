'use strict';
const mongoose = require('mongoose');
const bcrypt = require('bcryptjs');

const UserSchema = new mongoose.Schema({
    name: { 
        type: String, 
        required: true,
        trim: true 
    },
    email: { 
        type: String, 
        required: true, 
        unique: true,
        lowercase: true, 
        trim: true 
    },
    password: { 
        type: String, 
        required: true 
    },
    // --- CƠ CHẾ OTP ---
    otp: { type: String, default: null },
    otpExpires: { type: Date, default: null },
    isVerified: { type: Boolean, default: false },
    tempEmail: { type: String, default: null },
    // --- BẢO MẬT HỆ THỐNG ---
    loginAttempts: { type: Number, default: 0 },
    lockoutUntil: { type: Date, default: null }
}, { timestamps: true });

// --- MIDDLEWARE TỰ ĐỘNG HASH MẬT KHẨU (Phiên bản tối ưu) ---
// Không dùng tham số 'next' để tránh lỗi Runtime
UserSchema.pre('save', async function() {
    // Chỉ hash nếu mật khẩu mới được tạo hoặc bị thay đổi
    if (!this.isModified('password')) return;

    try {
        const salt = await bcrypt.genSalt(10);
        this.password = await bcrypt.hash(this.password, salt);
    } catch (error) {
        // Ném lỗi để Mongoose xử lý
        throw new Error('Lỗi mã hóa mật khẩu: ' + error.message);
    }
});

// --- HELPER METHOD ---
UserSchema.methods.isLocked = function() {
    return !!(this.lockoutUntil && this.lockoutUntil > Date.now());
};

const UserModel = mongoose.model('Users', UserSchema);
// Đăng ký alias 'User' để tương thích với ref: 'User' trong BoxModel.js
mongoose.model('User', UserSchema);
module.exports = UserModel;