'use strict';
const cron = require('node-cron');
const emailService = require('./emailServiceV2');
const Box = require('../models/BoxModel');
const Log = require('../models/LogModel');
const User = require('../models/UserModel'); // Đảm bảo model User được load và đăng ký trước khi populate

// Nhận vào 'io' từ server.js
const startCronJob = (io) => { 
    // SỬA: Chuyển thành '* * * * *' để quét MỖI PHÚT, không bỏ sót bất kỳ phút hẹn nào
    cron.schedule('* * * * *', async () => {
        try {
            const now = new Date();
            // Tính toán mốc thời gian của 10 phút trước (Làm khoảng trễ cữ đệm tránh báo động giả)
            const delayMinutes = 10;
            const targetTime = new Date(now.getTime() - delayMinutes * 60000);
            
            const targetFormatter = new Intl.DateTimeFormat('en-CA', {
                timeZone: 'Asia/Ho_Chi_Minh',
                hour: '2-digit', minute: '2-digit', hour12: false
            });
            const targetParts = targetFormatter.formatToParts(targetTime);
            const targetHour = targetParts.find(p => p.type === 'hour').value;
            const targetMinute = targetParts.find(p => p.type === 'minute').value;
            const checkTimeString = `${targetHour}:${targetMinute}`; // Mốc giờ cữ thuốc cần kiểm tra (ví dụ: 08:00 nếu hiện tại là 08:10)

            // Lấy tất cả các hộp thuốc đã được liên kết với người dùng
            const allBoxes = await Box.find({ ownerId: { $exists: true, $ne: null } }).populate('ownerId');

            for (const box of allBoxes) {
                if (!box.ownerId || !box.ownerId.email) continue;
                
                const userId = box.ownerId._id.toString(); 

                for (const comp of box.compartments) {
                    // Kiểm tra xem ngăn thuốc có đang bật không và có tên thuốc không
                    if (!comp.enabled || !comp.medicineName) continue;

                    // 1. Kiểm tra xem ĐÚNG khoảng thời gian trễ cữ (10 phút sau giờ hẹn) chưa
                    if (comp.scheduleTime !== checkTimeString) continue;

                    // 2. KIỂM TRA XEM ĐÃ UỐNG CHƯA (Quét Log trong 15 phút trở lại đây để kiểm tra cữ hẹn)
                    const hasLog = await Log.findOne({ 
                        boxId: box.boxId,
                        medicineName: { $regex: new RegExp(`^${comp.medicineName}$`, 'i') },
                        status: { $in: ['Đã uống', 'Đúng giờ'] },
                        timeLog: {
                            $gte: new Date(now.getTime() - 15 * 60000) 
                        }
                    });

                    // 3. Nếu CHƯA có log xác nhận sau 10 phút -> Kích hoạt gửi thông báo nhắc nhở trễ cữ
                    if (!hasLog) {
                        console.log(`⚠️ [Cron] Phát hiện trễ cữ quên uống thuốc: ${comp.medicineName} của user ${box.ownerId.email}`);
                        
                        const statusWithCondition = `Chưa uống [Chỉ dẫn: ${comp.mealNote}]`;

                        // 1. Gửi Email nhắc nhở (Bỏ await để không nghẽn mạch)
                        emailService.sendPillNotification(
                            box.ownerId.email,
                            comp.medicineName,
                            statusWithCondition
                        ).catch(err => console.error("❌ [Cron] Lỗi gửi email nhắc nhở:", err));

                        // 2. Bắn thông báo realtime qua Socket.io
                        io.to(userId).emit('new-notification', { 
                            message: `Cảnh báo: Đã trễ cữ 10 phút đối với lịch nhắc uống thuốc ${comp.medicineName} (${comp.mealNote}) nhưng chưa ghi nhận phản hồi từ thiết bị!`,
                            medicine: comp.medicineName,
                            time: comp.scheduleTime,
                            boxId: box.boxId
                        });
                    }
                }
            }
        } catch (error) {
            console.error('❌ [Cron Job Error]:', error);
        }
    });
};

module.exports = startCronJob;