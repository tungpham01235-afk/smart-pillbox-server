'use strict';
const User = require('../models/UserModel');
const Box = require('../models/BoxModel'); // Import BoxModel để tìm boxId của người dùng khi đăng nhập
const jwt = require('jsonwebtoken');
const bcrypt = require('bcryptjs');
const emailService = require('../services/emailServiceV2');

// --- HÀM ĐĂNG KÝ ---
const register = async (req, res) => {
    try {
        const { name, email, password } = req.body;
        if (!name || !email || !password) return res.status(400).json({ message: 'Vui lòng điền đủ thông tin!' });
        if (password.length < 5) return res.status(400).json({ message: 'Mật khẩu phải từ 5 ký tự trở lên!' });

        const emailLower = email.toLowerCase().trim();
        const existingUser = await User.findOne({ email: emailLower });

        if (existingUser && existingUser.isVerified) {
            return res.status(400).json({ message: 'Email này đã tồn tại!' });
        }

        const otp = Math.floor(100000 + Math.random() * 900000).toString();
        const otpExpires = new Date(Date.now() + 10 * 60 * 1000);

        if (existingUser) {
            existingUser.name = name;
            existingUser.password = password;
            existingUser.otp = otp;
            existingUser.otpExpires = otpExpires;
            await existingUser.save();
        } else {
            await User.create({ name, email: emailLower, password, otp, otpExpires });
        }

        await emailService.sendMail(
            emailLower,
            'Mã xác thực tài khoản',
            `<p>Xin chào ${name}, Mã OTP của bạn là: <strong>${otp}</strong>.</p>`
        );

        return res.status(201).json({ message: 'Đăng ký thành công! Vui lòng kiểm tra email.' });
    } catch (err) {
        return res.status(500).json({ message: 'Lỗi server: ' + err.message });
    }
};

// --- HÀM XÁC THỰC OTP ---
const verifyOTP = async (req, res) => {
    try {
        const { email, otp } = req.body;
        const user = await User.findOne({ email: email.toLowerCase().trim() });

        if (!user || user.otp !== otp || user.otpExpires < new Date()) {
            return res.status(400).json({ message: 'OTP không hợp lệ hoặc đã hết hạn!' });
        }

        user.isVerified = true;
        user.otp = null;
        user.otpExpires = null;
        await user.save();

        return res.json({ message: 'Xác thực thành công!' });
    } catch (err) {
        return res.status(500).json({ message: 'Lỗi xác thực: ' + err.message });
    }
};

// --- HÀM GỬI LẠI OTP ---
const resendOTP = async (req, res) => {
    try {
        const { email } = req.body;
        const user = await User.findOne({ email: email.toLowerCase().trim() });

        if (!user) return res.status(404).json({ message: 'Không tìm thấy người dùng!' });

        const otp = Math.floor(100000 + Math.random() * 900000).toString();
        user.otp = otp;
        user.otpExpires = new Date(Date.now() + 10 * 60 * 1000);
        await user.save();

        await emailService.sendMail(
            user.email,
            'Mã xác thực mới',
            `<p>Mã OTP mới của bạn là: <strong>${otp}</strong>.</p>`
        );

        return res.json({ message: 'Đã gửi lại mã OTP!' });
    } catch (err) {
        return res.status(500).json({ message: 'Lỗi gửi lại OTP: ' + err.message });
    }
};

// --- HÀM CẬP NHẬT THÔNG TIN ---
const updateProfile = async (req, res) => {
    try {
        const { name, password, oldPassword } = req.body;
        const userId = req.user.userId;

        const user = await User.findById(userId);
        if (!user) return res.status(404).json({ message: 'Không tìm thấy người dùng!' });

        if (name) user.name = name;

        if (password) {
            if (!oldPassword) {
                return res.status(400).json({ message: 'Vui lòng cung cấp mật khẩu cũ để thay đổi mật khẩu mới!' });
            }
            const isMatch = await bcrypt.compare(oldPassword, user.password);
            if (!isMatch) {
                return res.status(400).json({ message: 'Mật khẩu cũ không chính xác!' });
            }
            if (password === oldPassword) {
                return res.status(400).json({ message: 'Mật khẩu mới không được trùng với mật khẩu cũ!' });
            }
            if (password.length < 5) {
                return res.status(400).json({ message: 'Mật khẩu mới phải từ 5 ký tự trở lên!' });
            }
            user.password = password;
        }

        await user.save();
        return res.json({ message: 'Cập nhật thông tin thành công!', user: { name: user.name, email: user.email } });
    } catch (err) {
        return res.status(500).json({ message: 'Lỗi cập nhật: ' + err.message });
    }
};

// --- HÀM ĐĂNG NHẬP ---
const login = async (req, res) => {
    try {
        const { email, password } = req.body;
        if (!email || !password) return res.status(400).json({ message: 'Vui lòng nhập đủ thông tin!' });
        if (password.length < 5) return res.status(401).json({ message: 'Mật khẩu không hợp lệ!' });

        const user = await User.findOne({ email: email.toLowerCase().trim() });

        if (!user) return res.status(401).json({ message: 'Sai email hoặc mật khẩu!' });
        if (!user.isVerified) return res.status(403).json({ message: 'Tài khoản chưa xác thực!' });
        if (user.isLocked && user.isLocked()) return res.status(403).json({ message: 'Tài khoản tạm khóa!' });

        const isMatch = await bcrypt.compare(password, user.password);
        if (!isMatch) {
            user.loginAttempts += 1;
            if (user.loginAttempts >= 5) user.lockoutUntil = new Date(Date.now() + 15 * 60 * 1000);
            await user.save();
            return res.status(401).json({ message: 'Sai email hoặc mật khẩu!' });
        }

        user.loginAttempts = 0;
        user.lockoutUntil = null;
        await user.save();

        const token = jwt.sign({ userId: user._id }, process.env.JWT_SECRET, { expiresIn: '30d' });

        // Lấy thông tin BoxID đã liên kết với người dùng này (nếu có)
        const userBox = await Box.findOne({ ownerId: user._id });
        const boxId = userBox ? userBox.boxId : null;

        return res.status(200).json({
            token,
            user: {
                id: user._id,
                name: user.name,
                email: user.email,
                boxId: boxId
            }
        });
    } catch (err) {
        return res.status(500).json({ message: 'Lỗi hệ thống: ' + err.message });
    }
};

// --- HÀM LẤY THÔNG TIN HỒ SƠ & BOX_ID ĐỒNG BỘ ---
const getProfile = async (req, res) => {
    try {
        const userId = req.user.userId;
        const user = await User.findById(userId);
        if (!user) return res.status(404).json({ message: 'Không tìm thấy người dùng!' });

        // Lấy thông tin BoxID đã liên kết với người dùng này (nếu có)
        const Box = require('../models/BoxModel'); // Require động tránh circular dependency
        const userBox = await Box.findOne({ ownerId: user._id });
        const boxId = userBox ? userBox.boxId : null;

        return res.json({
            user: {
                id: user._id,
                name: user.name,
                email: user.email,
                boxId: boxId
            }
        });
    } catch (err) {
        return res.status(500).json({ message: 'Lỗi hệ thống: ' + err.message });
    }
};

// --- HÀM YÊU CẦU ĐỔI EMAIL ---
const requestEmailChange = async (req, res) => {
    try {
        const { newEmail } = req.body;
        const userId = req.user.userId;

        if (!newEmail) return res.status(400).json({ message: 'Vui lòng cung cấp email mới!' });

        const emailLower = newEmail.toLowerCase().trim();

        // Kiểm tra xem email mới đã được sử dụng bởi ai khác chưa
        const emailExists = await User.findOne({ email: emailLower, _id: { $ne: userId } });
        if (emailExists) return res.status(400).json({ message: 'Email này đã được sử dụng bởi tài khoản khác!' });

        const user = await User.findById(userId);
        if (!user) return res.status(404).json({ message: 'Không tìm thấy người dùng!' });

        if (user.email === emailLower) {
            return res.status(400).json({ message: 'Email mới trùng với email hiện tại!' });
        }

        // Tạo mã OTP
        const otp = Math.floor(100000 + Math.random() * 900000).toString();
        const otpExpires = new Date(Date.now() + 10 * 60 * 1000); // 10 phút

        user.tempEmail = emailLower;
        user.otp = otp;
        user.otpExpires = otpExpires;
        await user.save();

        await emailService.sendMail(
            emailLower,
            'Xác thực thay đổi Email - Smart Pillbox',
            `<p>Xin chào ${user.name},</p>
             <p>Bạn đã yêu cầu thay đổi email nhận thông báo của tủ thuốc thông minh sang địa chỉ này.</p>
             <p>Mã OTP xác thực của bạn là: <strong>${otp}</strong>.</p>
             <p>Mã này có hiệu lực trong vòng 10 phút. Nếu bạn không yêu cầu thay đổi này, hãy bỏ qua email này.</p>`
        );

        return res.json({ message: 'Mã OTP xác thực đã được gửi về email mới của bạn!' });
    } catch (err) {
        return res.status(500).json({ message: 'Lỗi gửi yêu cầu đổi email: ' + err.message });
    }
};

// --- HÀM XÁC THỰC ĐỔI EMAIL ---
const verifyEmailChange = async (req, res) => {
    try {
        const { otp } = req.body;
        const userId = req.user.userId;

        const user = await User.findById(userId);
        if (!user) return res.status(404).json({ message: 'Không tìm thấy người dùng!' });

        if (!user.tempEmail || user.otp !== otp || !user.otpExpires || user.otpExpires < new Date()) {
            return res.status(400).json({ message: 'Mã OTP không chính xác hoặc đã hết hạn!' });
        }

        const newEmail = user.tempEmail;

        // Một lần nữa kiểm tra xem email có bị trùng với ai khác trong lúc đang chờ không
        const emailExists = await User.findOne({ email: newEmail, _id: { $ne: userId } });
        if (emailExists) return res.status(400).json({ message: 'Email này đã bị tài khoản khác sử dụng trong lúc xác thực!' });

        user.email = newEmail;
        user.tempEmail = null;
        user.otp = null;
        user.otpExpires = null;
        await user.save();

        return res.json({
            message: 'Đổi email thành công!',
            user: { name: user.name, email: user.email }
        });
    } catch (err) {
        return res.status(500).json({ message: 'Lỗi xác thực đổi email: ' + err.message });
    }
};

module.exports = { register, login, updateProfile, verifyOTP, resendOTP, getProfile, requestEmailChange, verifyEmailChange };