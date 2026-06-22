require('dotenv').config();
const express = require('express');
const path = require('path');
const cors = require('cors');
const http = require('http'); // 1. Import module http để tạo server riêng
const app = express();

// 2. Tạo http server từ express app
const server = http.createServer(app);

// 3. Khởi tạo Socket.io gắn vào http server
const io = require('socket.io')(server, {
    cors: { origin: "*" } // Cho phép kết nối từ mọi nguồn
});

// Gán io vào app để các controller (như BoxController) có thể truy cập được
app.set('io', io);

// --- IMPORT DỊCH VỤ & MODELS ---
const startCronJob = require('./api/services/cronService'); 
const Box = require('./api/models/BoxModel'); // BỔ SUNG: Import model để lưu thuốc vào Database

// 1. Cấu hình middleware
app.use(cors()); 
app.use(express.json()); 

// Phục vụ Service Worker với Content-Type và Cache-Control đúng chuẩn PWA
app.get('/service-worker.js', (req, res) => {
    res.setHeader('Content-Type', 'application/javascript');
    res.setHeader('Cache-Control', 'no-cache, no-store, must-revalidate'); // SW phải luôn tươi
    res.setHeader('Service-Worker-Allowed', '/');
    res.sendFile(path.join(__dirname, 'service-worker.js'));
});

// Phục vụ Manifest với Content-Type đúng
app.get('/manifest.json', (req, res) => {
    res.setHeader('Content-Type', 'application/manifest+json');
    res.setHeader('Cache-Control', 'no-cache');
    res.sendFile(path.join(__dirname, 'manifest.json'));
});

// Phục vụ tài nguyên tĩnh công khai (icon, html, v.v.)
app.use(express.static(path.join(__dirname, 'public'), {
    setHeaders: (res, filePath) => {
        // Icons: cache dài hạn
        if (filePath.endsWith('.png') || filePath.endsWith('.ico')) {
            res.setHeader('Cache-Control', 'public, max-age=604800');
        }
        // HTML: không cache để đảm bảo nội dung mới nhất
        if (filePath.endsWith('.html')) {
            res.setHeader('Cache-Control', 'no-cache');
        }
    }
}));

// 2. Kết nối Database
require('./api/db');

// 3. Nhúng routes hệ thống của bạn (Đăng nhập, Liên kết tủ, Xem Logs)
const routes = require('./api/routes'); 
routes(app); 


// =========================================================================
// --- ĐÃ CẬP NHẬT: ĐỒNG BỘ API MEDICINES VỚI MONGOOSE DATABASE ---
// =========================================================================

// API 1: Lấy danh sách lịch uống thuốc từ các ngăn (compartments)
app.get('/api/medicines', async (req, res) => {
    try {
        let { boxId } = req.query;
        
        if (boxId) {
            boxId = boxId.trim().toLowerCase().replace(/\s+/g, '_');
            const box = await Box.findOne({ boxId });
            if (!box) return res.status(200).json([]);

            // XÁC THỰC BẢO MẬT: 
            // 1. Kiểm tra nếu có x-device-key hợp lệ (thiết bị ESP32)
            const deviceKey = req.headers['x-device-key'];
            const isDevice = deviceKey && deviceKey.trim() === (process.env.DEVICE_SECRET_KEY || '').trim();
            
            if (!isDevice) {
                // 2. Nếu không phải thiết bị, yêu cầu token của chủ sở hữu
                const authHeader = req.headers['authorization'];
                if (!authHeader) {
                    return res.status(403).json({ message: "Không được phép truy cập!" });
                }
                const token = authHeader.split(' ')[1];
                const jwt = require('jsonwebtoken');
                try {
                    const decoded = jwt.verify(token, process.env.JWT_SECRET);
                    if (!box.ownerId || box.ownerId.toString() !== decoded.userId.toString()) {
                        return res.status(403).json({ message: "Bạn không có quyền truy cập hộp thuốc này!" });
                    }
                } catch (e) {
                    return res.status(403).json({ message: "Token không hợp lệ hoặc hết hạn!" });
                }
            }

            const list = box.compartments.map(comp => ({
                id: comp._id,
                compartmentId: comp.id,
                boxId: box.boxId,
                name: comp.medicineName,
                condition: comp.mealNote,
                time: comp.scheduleTime,
                active: comp.enabled,
                isMedicinePresent: comp.isMedicinePresent
            }));
            return res.status(200).json(list);
        }
        
        // Trả về danh sách trống nếu không truyền boxId để tránh lộ dữ liệu chéo giữa các tủ thuốc
        return res.status(200).json([]);
    } catch (error) {
        res.status(500).json({ message: "Lỗi lấy danh sách thuốc!", error: error.message });
    }
});

// API 2: Nhận và lưu/cập nhật cấu hình ngăn thuốc trực tiếp vào MongoDB
app.post('/api/medicines', async (req, res) => {
    try {
        let { boxId, compartmentId, name, condition, time, enabled } = req.body;
        if (boxId) {
            boxId = boxId.trim().toLowerCase().replace(/\s+/g, '_');
        }

        if (!compartmentId) {
            return res.status(400).json({ message: "Vui lòng chọn ngăn uống thuốc (1, 2, hoặc 3)!" });
        }

        // Định vị hộp thuốc mục tiêu
        let targetBox;
        if (boxId) {
            targetBox = await Box.findOne({ boxId });
        } else {
            targetBox = await Box.findOne(); 
        }

        if (!targetBox) {
            return res.status(404).json({ message: "Không tìm thấy hộp thuốc nào khả dụng trong DB!" });
        }

        // XÁC THỰC BẢO MẬT: Yêu cầu token và người dùng phải là chủ sở hữu của hộp thuốc
        const authHeader = req.headers['authorization'];
        if (!authHeader) {
            return res.status(403).json({ message: "Không được phép truy cập!" });
        }
        const token = authHeader.split(' ')[1];
        const jwt = require('jsonwebtoken');
        try {
            const decoded = jwt.verify(token, process.env.JWT_SECRET);
            if (!targetBox.ownerId || targetBox.ownerId.toString() !== decoded.userId.toString()) {
                return res.status(403).json({ message: "Bạn không có quyền thay đổi cấu hình hộp thuốc này!" });
            }
        } catch (e) {
            return res.status(403).json({ message: "Token không hợp lệ hoặc hết hạn!" });
        }

        const compIdx = parseInt(compartmentId) - 1;
        if (compIdx < 0 || compIdx >= 3) {
            return res.status(400).json({ message: "ID ngăn thuốc không hợp lệ (1-3)!" });
        }

        // Cập nhật cấu hình ngăn tương ứng
        if (targetBox.compartments && targetBox.compartments[compIdx]) {
            targetBox.compartments[compIdx].medicineName = name !== undefined ? name : targetBox.compartments[compIdx].medicineName;
            targetBox.compartments[compIdx].scheduleTime = time !== undefined ? time : targetBox.compartments[compIdx].scheduleTime;
            targetBox.compartments[compIdx].mealNote = condition !== undefined ? condition : targetBox.compartments[compIdx].mealNote;
            targetBox.compartments[compIdx].enabled = enabled !== undefined ? enabled : true;
        }

        await targetBox.save();
        const savedComp = targetBox.compartments[compIdx];

        console.log(`📌 [API Medicines] Đã nạp thành công lịch thuốc vào DB cho Box [${targetBox.boxId}]:`, savedComp);
        
        res.status(201).json({ 
            message: "Lưu lịch trình vào Database thành công!", 
            data: {
                id: savedComp._id,
                compartmentId: savedComp.id,
                boxId: targetBox.boxId,
                name: savedComp.medicineName,
                condition: savedComp.mealNote,
                time: savedComp.scheduleTime,
                active: savedComp.enabled
            } 
        });
    } catch (error) {
        res.status(500).json({ message: "Lỗi xử lý lưu lịch trình!", error: error.message });
    }
});
// =========================================================================


// --- XỬ LÝ SOCKET CONNECTION (Cơ chế Room thông báo thời gian thực) ---
io.on('connection', (socket) => {
    console.log('✅ Một thiết bị đã kết nối Socket thành công (ID):', socket.id);

    // Lắng nghe yêu cầu "vào phòng" riêng dựa trên ID người dùng để điều hướng cảnh báo chính xác
    socket.on('join-room', (userId) => {
        if (userId) {
            socket.join(userId); 
            console.log(`👤 Người dùng [${userId}] đã tham gia phòng nhận thông báo riêng.`);
        }
    });
});

// --- KHỞI ĐỘNG CRON JOB ---
// Tiến hành kích hoạt bộ kiểm tra cữ thuốc tự động (đã nâng cấp quét mỗi phút)
startCronJob(io); 

// Route mặc định tải giao diện điều khiển (Dashboard)
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// 4. Khởi động server (Sử dụng server.listen thay vì app.listen để kích hoạt cổng truyền thông mạng của Socket)
const PORT = process.env.PORT || 3000;
server.listen(PORT, '0.0.0.0', () => {
    console.log(`🚀 [Server] Hệ thống IoT Tủ Thuốc chạy tại cổng: ${PORT}`);
    console.log(`⏰ [Cron] Tiến trình kiểm tra cữ thuốc chạy song song thành công!`);
});