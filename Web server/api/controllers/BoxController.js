'use strict';
const Box = require('../models/BoxModel');
const Log = require('../models/LogModel');
const emailService = require('../services/emailServiceV2'); 
const User = require('../models/UserModel'); // Đảm bảo model User được load trước khi populate ownerId

const sanitizeBoxId = (id) => (id || '').trim().toLowerCase().replace(/\s+/g, '_');

module.exports = {
    // 1. Liên kết tủ thuốc (QR Code) - Cập nhật khởi tạo 3 compartments
    bindBox: async (req, res) => {
        try {
            const boxId = sanitizeBoxId(req.body.boxId);
            const userId = req.user.userId;

            // 1. Kiểm tra xem người dùng này đã sở hữu một tủ thuốc nào khác chưa (Quy tắc bảo mật 1-đối-1)
            const existingUserBox = await Box.findOne({ ownerId: userId });
            if (existingUserBox && existingUserBox.boxId !== boxId) {
                return res.status(400).json({ message: 'Tài khoản của bạn đã liên kết với một tủ thuốc khác rồi!' });
            }

            let box = await Box.findOne({ boxId });

            const defaultCompartments = [
                { id: 1, medicineName: "Thuốc dạ dày", scheduleTime: "07:00", mealNote: "Sau ăn", enabled: true },
                { id: 2, medicineName: "Vitamin C", scheduleTime: "12:00", mealNote: "Sau ăn", enabled: true },
                { id: 3, medicineName: "Thuốc huyết áp", scheduleTime: "20:00", mealNote: "Trước khi ngủ", enabled: true }
            ];

            if (!box) {
                const newBox = new Box({ boxId, ownerId: userId, compartments: defaultCompartments });
                await newBox.save();
                return res.status(201).json({ message: 'Đã khởi tạo và liên kết tủ thuốc!', box: newBox });
            }

            if (box.ownerId) {
                if (box.ownerId.toString() === userId.toString()) {
                    return res.json({ message: 'Thiết bị đã được liên kết với tài khoản của bạn từ trước!', box });
                }
                return res.status(400).json({ message: 'Thiết bị này đã có chủ sở hữu khác!' });
            }

            box.ownerId = userId;
            if (!box.compartments || box.compartments.length === 0) {
                box.compartments = defaultCompartments;
            }
            await box.save();
            return res.json({ message: 'Liên kết thiết bị thành công!', box });
        } catch (err) {
            return res.status(500).json({ error: err.message });
        }
    },

    // 2. Lấy cấu hình (cho App/User Dashboard)
    getBoxConfig: async (req, res) => {
        try {
            const boxId = sanitizeBoxId(req.params.boxId);
            const box = await Box.findOne({ boxId, ownerId: req.user.userId });
            if (!box) return res.status(404).json({ message: 'Không tìm thấy thiết bị hoặc không có quyền truy cập!' });
            return res.json(box);
        } catch (err) {
            return res.status(500).json({ error: err.message });
        }
    },

    // 3. Cập nhật cấu hình ngăn thuốc từ Dashboard
    updateAlarms: async (req, res) => {
        try {
            const boxId = sanitizeBoxId(req.params.boxId);
            const { compartments } = req.body;

            const updatedBox = await Box.findOneAndUpdate(
                { boxId, ownerId: req.user.userId },
                { $set: { compartments: compartments } },
                { new: true }
            );

            if (!updatedBox) return res.status(404).json({ message: 'Cập nhật thất bại hoặc không tìm thấy thiết bị!' });
            return res.json({ message: 'Cập nhật lịch thành công!', compartments: updatedBox.compartments });
        } catch (err) {
            return res.status(500).json({ error: err.message });
        }
    },

    // 3.5 Giả lập báo động từ Dashboard để Test (Không yêu cầu x-device-key)
    testAlarm: async (req, res) => {
        try {
            const boxId = sanitizeBoxId(req.params.boxId);
            const { compartmentId, status } = req.body;
            const userId = req.user.userId;

            // Tìm thông tin thiết bị và nạp thông tin chủ sở hữu (User) để lấy Email
            const box = await Box.findOne({ boxId, ownerId: userId }).populate('ownerId');
            
            if (!box) {
                return res.status(404).json({ message: 'Không tìm thấy thiết bị hoặc bạn không có quyền truy cập!' });
            }

            const compId = parseInt(compartmentId) || 1;
            const compIdx = compId - 1;
            let medicineName = `Ngăn ${compId}`;
            let conditionDetails = 'Khác';
            if (box.compartments && box.compartments[compIdx]) {
                medicineName = box.compartments[compIdx].medicineName || medicineName;
                conditionDetails = box.compartments[compIdx].mealNote || 'Khác';
            }

            const finalStatus = status || 'Chưa uống';

            // HỖ TRỢ GIẢ LẬP CẢM BIẾN RIÊNG BIỆT (Không tạo Log và không gửi Email báo động)
            if (finalStatus === 'CoThuoc' || finalStatus === 'KhayTrong') {
                if (box.compartments && box.compartments[compIdx]) {
                    box.compartments[compIdx].isMedicinePresent = (finalStatus === 'CoThuoc');
                    await box.save();
                }
                
                if (box.ownerId) {
                    const io = req.app.get('io');
                    const socketUserId = box.ownerId._id.toString(); 
                    io.to(socketUserId).emit('new-notification', { 
                        message: `[TEST] Cảm biến ngăn ${compId} (${medicineName}) cập nhật: ${finalStatus === 'CoThuoc' ? 'Có thuốc' : 'Khay trống'}`,
                        time: new Date(),
                        boxId: boxId
                    });
                }
                return res.json({ 
                    success: true,
                    message: `Đã giả lập cảm biến khay ${compId}: ${finalStatus === 'CoThuoc' ? 'Có thuốc' : 'Khay trống'}!` 
                });
            }

            // Tạo và lưu log giả lập báo động thông thường
            const newLog = new Log({
                boxId,
                medicineName: medicineName,
                status: finalStatus,
                timeLog: new Date(),
                note: `[TEST] Thiết bị giả lập báo động tại khay số ${compId} (${medicineName})`
            });
            await newLog.save();

            // Tự động giả lập cập nhật trạng thái cảm biến vật lý trong DB để đồng bộ UI
            if (box.compartments && box.compartments[compIdx]) {
                if (finalStatus === 'Đã uống' || finalStatus === 'Mở tủ sai') {
                    box.compartments[compIdx].isMedicinePresent = false; // Trống do nhấc thuốc ra
                } else if (finalStatus === 'Chưa uống') {
                    box.compartments[compIdx].isMedicinePresent = true;  // Có thuốc nhưng chưa lấy
                }
            }

            box.triggerTestAlarm = compId; // Kích hoạt test còi/đèn vật lý trên mạch thật
            await box.save();

            // Tiến hành gửi thông báo tới người dùng nếu thiết bị đã kích hoạt chủ sở hữu
            if (box.ownerId) {
                const finalStatusString = `${finalStatus} [Chỉ dẫn: ${conditionDetails}]`;

                // 1. Gửi Email thông báo (không await để tránh blocking request)
                if (box.ownerId.email) {
                    emailService.sendPillNotification(box.ownerId.email, medicineName, finalStatusString)
                        .catch(err => console.error("❌ Hệ thống gặp sự cố gửi email test:", err));
                }

                // 2. Bắn tín hiệu Socket real-time về phòng (Room) riêng của User trên giao diện Web
                const io = req.app.get('io');
                const socketUserId = box.ownerId._id.toString(); 

                io.to(socketUserId).emit('new-notification', { 
                    message: `[TEST] Hộp thuốc báo cáo: Đã ghi nhận tác động tại ngăn ${compId} (${medicineName}) - Trạng thái: ${finalStatus} (${conditionDetails})`,
                    time: new Date(),
                    boxId: boxId
                });
            }

            return res.json({ 
                success: true,
                message: 'Đã gửi yêu cầu giả lập báo động (email & app) thành công!' 
            });
        } catch (err) {
            return res.status(500).json({ error: err.message });
        }
    },

    // 3.6 Đặt lại Wi-Fi thiết bị từ xa (Xóa /wifi.json và Restart ESP32)
    resetDeviceWifi: async (req, res) => {
        try {
            const boxId = sanitizeBoxId(req.params.boxId || req.body.boxId);
            const userId = req.user.userId;

            // Tìm thiết bị thuộc quyền sở hữu của User
            const box = await Box.findOne({ boxId, ownerId: userId });
            if (!box) {
                return res.status(404).json({ success: false, message: 'Không tìm thấy thiết bị hoặc bạn không có quyền!' });
            }

            box.resetWifi = true; // Kích hoạt cờ reset Wi-Fi
            await box.save();

            // Gửi thông báo WebSocket về client
            const io = req.app.get('io');
            const socketUserId = userId.toString(); 
            io.to(socketUserId).emit('new-notification', { 
                message: `Đã gửi yêu cầu đặt lại Wi-Fi tới thiết bị (${boxId}). Thiết bị sẽ nhận lệnh và reset trong chu kỳ sync kế tiếp.`,
                time: new Date(),
                boxId: boxId
            });

            return res.json({ 
                success: true, 
                message: 'Đã gửi yêu cầu đặt lại Wi-Fi thiết bị thành công!' 
            });
        } catch (err) {
            return res.status(500).json({ success: false, error: err.message });
        }
    },

    // 4. Đồng bộ Log từ ESP32 & Gửi thông báo
    savePillLog: async (req, res) => {
        try {
            const boxId = sanitizeBoxId(req.body.boxId);
            const { compartmentId, status } = req.body;

            // Tìm thông tin thiết bị và nạp thông tin chủ sở hữu (User) để lấy Email
            const box = await Box.findOne({ boxId }).populate('ownerId');
            
            if (!box) {
                console.warn(`⚠️ Cảnh báo: Thiết bị gửi dữ liệu từ boxId không tồn tại: ${boxId}`);
                return res.status(404).json({ message: 'Không tìm thấy thiết bị hoặc ID không hợp lệ!' });
            }

            // Cập nhật hoạt động cuối cùng của thiết bị
            box.lastActive = new Date();
            await box.save();

            const compIdx = compartmentId - 1;
            let medicineName = `Ngăn ${compartmentId}`;
            let conditionDetails = 'Khác';
            if (box.compartments && box.compartments[compIdx]) {
                medicineName = box.compartments[compIdx].medicineName || medicineName;
                conditionDetails = box.compartments[compIdx].mealNote || 'Khác';
            }

            // Tạo và lưu log từ thiết bị gửi về
            const newLog = new Log({
                boxId,
                medicineName: medicineName,
                status: status || 'Đã uống',
                timeLog: new Date(),
                note: `Thiết bị ghi nhận tại khay số ${compartmentId} (${medicineName})`
            });
            await newLog.save();

            // Tiến hành gửi thông báo tới người dùng nếu thiết bị đã kích hoạt chủ sở hữu
            if (box.ownerId) {
                const finalStatusString = `${status} [Chỉ dẫn: ${conditionDetails}]`;

                // 1. Gửi Email thông báo (không await để tránh blocking request)
                emailService.sendPillNotification(box.ownerId.email, medicineName, finalStatusString)
                    .catch(err => console.error("❌ Hệ thống gặp sự cố gửi email xác thực:", err));

                // 2. Bắn tín hiệu Socket real-time về phòng (Room) riêng của User trên giao diện Web
                const io = req.app.get('io');
                const userId = box.ownerId._id.toString(); 

                let displayMessage = `Hộp thuốc báo cáo: Đã ghi nhận tác động tại ngăn ${compartmentId} (${medicineName}) - Trạng thái: ${status} (${conditionDetails})`;
                if (status === 'Chưa uống') {
                    displayMessage = `🔔 Đến giờ uống thuốc: Ngăn ${compartmentId} (${medicineName}) - Trạng thái: Chưa uống (${conditionDetails})`;
                } else if (status === 'Đã uống' || status === 'Đúng giờ') {
                    displayMessage = `✅ Xác nhận đã uống thuốc tại ngăn ${compartmentId} (${medicineName}) - Trạng thái: Đúng giờ`;
                } else if (status === 'Mở tủ sai') {
                    displayMessage = `⚠️ Cảnh báo: Đã ghi nhận mở khay thuốc sai cữ tại ngăn ${compartmentId} (${medicineName})!`;
                } else if (status === 'Bỏ lỡ') {
                    displayMessage = `🔴 Cảnh báo trễ cữ: Bạn đã bỏ lỡ lịch nhắc uống thuốc ${medicineName} (${conditionDetails})!`;
                }

                io.to(userId).emit('new-notification', { 
                    message: displayMessage,
                    time: new Date(),
                    boxId: boxId
                });
            }

            return res.json({ 
                success: true,
                message: 'Đồng bộ log thành công!'
            });
        } catch (err) {
            return res.status(500).json({ error: err.message });
        }
    },

    // 5. Lấy lịch sử Logs (đã có kiểm tra quyền sở hữu) - GIỮ NGUYÊN
    getBoxLogs: async (req, res) => {
        try {
            const boxId = sanitizeBoxId(req.params.boxId);
            const userId = req.user.userId;

            const box = await Box.findOne({ boxId, ownerId: userId });
            if (!box) {
                return res.status(403).json({ message: 'Bạn không có quyền xem dữ liệu của thiết bị này!' });
            }

            const logs = await Log.find({ boxId })
                                  .sort({ timeLog: -1 })
                                  .limit(50);
            return res.json(logs);
        } catch (err) {
            return res.status(500).json({ error: err.message });
        }
    },

    // 6. API dành riêng cho thiết bị ESP32 lấy cấu hình 3 ngăn & Đồng bộ trạng thái cảm biến
    getDeviceConfig: async (req, res) => {
        try {
            const boxId = sanitizeBoxId(req.query.boxId);
            const { s1, s2, s3 } = req.query;
            if (!boxId) {
                return res.status(400).json({ success: false, message: 'Thiếu tham số boxId!' });
            }

            const box = await Box.findOne({ boxId });
            if (!box) {
                return res.status(404).json({ success: false, message: 'Không tìm thấy thiết bị!' });
            }

            // Cập nhật hoạt động cuối cùng của thiết bị
            box.lastActive = new Date();

            // Nếu thiết bị gửi trạng thái cảm biến lên, cập nhật vào database
            let isChanged = false;
            if (s1 !== undefined && box.compartments[0]) {
                const s1Bool = s1 === 'true';
                if (box.compartments[0].isMedicinePresent !== s1Bool) {
                    box.compartments[0].isMedicinePresent = s1Bool;
                    isChanged = true;
                }
            }
            if (s2 !== undefined && box.compartments[1]) {
                const s2Bool = s2 === 'true';
                if (box.compartments[1].isMedicinePresent !== s2Bool) {
                    box.compartments[1].isMedicinePresent = s2Bool;
                    isChanged = true;
                }
            }
            if (s3 !== undefined && box.compartments[2]) {
                const s3Bool = s3 === 'true';
                if (box.compartments[2].isMedicinePresent !== s3Bool) {
                    box.compartments[2].isMedicinePresent = s3Bool;
                    isChanged = true;
                }
            }

            if (isChanged) {
                await box.save();
                
                // Bắn thông báo Socket realtime về trạng thái khay thay đổi
                if (box.ownerId) {
                    const io = req.app.get('io');
                    const userId = box.ownerId.toString();
                    io.to(userId).emit('new-notification', {
                        message: `Cập nhật trạng thái khay thuốc: Ngăn 1 (${box.compartments[0].isMedicinePresent ? 'Có thuốc' : 'Trống'}), Ngăn 2 (${box.compartments[1].isMedicinePresent ? 'Có thuốc' : 'Trống'}), Ngăn 3 (${box.compartments[2].isMedicinePresent ? 'Có thuốc' : 'Trống'})`,
                        time: new Date(),
                        boxId: boxId
                    });
                }
            } else {
                // Nếu không có thay đổi cảm biến, ta vẫn lưu lastActive
                await box.save();
            }

            // Kiểm tra và lấy cờ test còi/đèn vật lý
            const triggerVal = box.triggerTestAlarm || 0;
            if (triggerVal > 0) {
                box.triggerTestAlarm = 0;
            }

            // Kiểm tra và lấy cờ reset Wi-Fi
            const resetWifiVal = box.resetWifi || false;
            if (resetWifiVal) {
                box.resetWifi = false;
            }

            // Lưu thay đổi cờ (nếu có)
            if (triggerVal > 0 || resetWifiVal) {
                await box.save();
            }

            // Bắn tín hiệu trạng thái kết nối Online của thiết bị thông qua Socket
            if (box.ownerId) {
                const io = req.app.get('io');
                const userId = box.ownerId.toString();
                io.to(userId).emit('device-status', {
                    boxId: boxId,
                    isOnline: true,
                    lastActive: box.lastActive
                });
            }

            return res.json({
                success: true,
                compartments: box.compartments,
                triggerTestAlarm: triggerVal,
                resetWifi: resetWifiVal
            });
        } catch (err) {
            return res.status(500).json({ success: false, error: err.message });
        }
    }
};