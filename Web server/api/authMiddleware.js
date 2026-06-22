'use strict';
const jwt = require('jsonwebtoken');

module.exports = function(req, res, next) {
    // 1. Lấy token từ header
    const authHeader = req.headers['authorization'];
    
    // 2. Kiểm tra format "Bearer <token>"
    const token = authHeader && authHeader.startsWith('Bearer ') 
        ? authHeader.split(' ')[1] 
        : null;

    if (!token) {
        return res.status(401).json({ 
            success: false, 
            message: 'Truy cập bị từ chối: Vui lòng đăng nhập để tiếp tục.' 
        });
    }

    try {
        // 3. Giải mã và xác thực
        // JWT_SECRET phải khớp với biến môi trường bạn dùng khi tạo token ở AuthController
        const decoded = jwt.verify(token, process.env.JWT_SECRET);
        
        // 4. Gán thông tin user vào request
        // decoded chính là object { userId: "..." } mà bạn đã sign ở AuthController
        req.user = decoded; 
        
        next();
    } catch (err) {
        // Xử lý chi tiết lỗi để client biết đường mà xử lý (ví dụ: yêu cầu login lại)
        if (err.name === 'TokenExpiredError') {
            return res.status(401).json({ 
                success: false, 
                message: 'Phiên đăng nhập đã hết hạn. Vui lòng đăng nhập lại.' 
            });
        }
        
        return res.status(403).json({ 
            success: false, 
            message: 'Token không hợp lệ hoặc đã bị thay đổi.' 
        });
    }
};