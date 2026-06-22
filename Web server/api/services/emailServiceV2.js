'use strict';
const { Resend } = require('resend');

// Khởi tạo một lần duy nhất (Dùng dummy key nếu chưa có env key để tránh crash khi deploy)
const resend = new Resend(process.env.RESEND_API_KEY || 're_dummykeyforstartup123456');
const SENDER = process.env.SENDER_EMAIL || 'no-reply@smart-pillbox.xyz';

const sendMail = async (to, subject, htmlContent) => {
    // Chỉ kiểm tra config khi gửi, không cần log dài dòng mỗi lần gọi
    if (!process.env.RESEND_API_KEY) {
        throw new Error("Missing RESEND_API_KEY in environment variables");
    }

    const payload = {
        from: SENDER,
        to: to,
        subject: subject,
        html: htmlContent
    };

    try {
        const data = await resend.emails.send(payload);
        return data;
    } catch (error) {
        console.error("❌ Lỗi gửi Mail:", error.message);
        throw error;
    }
};

module.exports = {
    sendMail,

    sendPillNotification: async (to, medicineName, status) => {
        let subject = "";
        let content = "";

        // 1. Ép định dạng thời gian chuẩn theo múi giờ Việt Nam (Asia/Ho_Chi_Minh) cho Email
        const formattedTime = new Date().toLocaleString('vi-VN', { timeZone: 'Asia/Ho_Chi_Minh' });

        // 2. Phân loại trạng thái thông minh
        const isTaken = typeof status === 'string' && (status.includes('Đã uống') || status.includes('Đúng giờ'));
        const isWrong = typeof status === 'string' && (status.includes('Mở tủ sai') || status.includes('Sai cữ'));

        // 3. Bóc tách thông minh chuỗi Chỉ dẫn y tế (nếu có)
        let conditionText = "";
        if (typeof status === 'string' && status.includes('[Chỉ dẫn:')) {
            const match = status.match(/\[Chỉ dẫn:\s*(.*?)\]/);
            if (match && match[1]) {
                conditionText = `<p><b>Huong dan dung:</b> <span style="color: #e65100; font-weight: bold;">${match[1]}</span></p>`;
            }
        }

        // 4. Xây dựng template Email phù hợp từng trường hợp (Loại bỏ emoji, dùng text đơn giản để tránh bộ lọc Spam)
        if (isTaken) {
            subject = `Xac nhan: Da uong thuoc ${medicineName} dung gio`;
            content = `
                <div style="font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto; border: 1px solid #e0e0e0; padding: 25px; border-radius: 12px; box-shadow: 0 4px 10px rgba(0,0,0,0.05);">
                    <h2 style="color: #2e7d32; margin-top: 0; border-bottom: 2px solid #2e7d32; padding-bottom: 10px;">Smart Pillbox - Xac Nhan</h2>
                    <p style="font-size: 15px;">Hệ thống ghi nhận bạn đã lấy thuốc và uống thành công.</p>
                    <div style="background-color: #f1f8e9; padding: 15px; border-radius: 8px; margin: 20px 0;">
                        <p style="margin: 5px 0;"><b>Ten thuoc:</b> <span style="font-size: 16px; color: #1b5e20; font-weight: bold;">${medicineName}</span></p>
                        ${conditionText}
                        <p style="margin: 5px 0;"><b>Thoi gian:</b> ${formattedTime}</p>
                    </div>
                    <p style="color: #555; font-style: italic; font-size: 13px; text-align: center; margin-top: 25px;">Cảm ơn bạn đã tuân thủ đúng lịch trình điều trị. Chúc bạn luôn mạnh khỏe!</p>
                </div>
            `;
        } else if (isWrong) {
            subject = `Thong bao: Mo sai ngan thuoc ${medicineName}`;
            content = `
                <div style="font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto; border: 1px solid #ffcc80; padding: 25px; border-radius: 12px; box-shadow: 0 4px 10px rgba(0,0,0,0.05);">
                    <h2 style="color: #e65100; margin-top: 0; border-bottom: 2px solid #e65100; padding-bottom: 10px;">Smart Pillbox - Canh Bao Loi</h2>
                    <p style="font-size: 15px; color: #e65100; font-weight: bold;">Cảnh báo: Phát hiện thao tác nhấc nhầm ngăn thuốc ngoài cữ hẹn!</p>
                    <p style="font-size: 14px;">Hệ thống nhận thấy khay thuốc chứa <b>${medicineName}</b> đã bị nhấc ra khi chưa đến giờ hoặc sai lịch trình chỉ định.</p>
                    <div style="background-color: #fff3e0; padding: 15px; border-radius: 8px; margin: 20px 0; border-left: 4px solid #ff9800;">
                        <p style="margin: 5px 0;"><b>Ngan tac dong:</b> <span style="font-size: 16px; color: #e65100; font-weight: bold;">${medicineName}</span></p>
                        <p style="margin: 5px 0;"><b>Thoi gian:</b> ${formattedTime}</p>
                    </div>
                    <p style="color: #d84315; font-weight: bold; font-size: 14px;">Vui lòng kiểm tra lại thiết bị ngay để tránh việc uống nhầm thuốc hoặc nhầm liều lượng!</p>
                </div>
            `;
        } else {
            subject = `Nhac nho: Den gio uong thuoc ${medicineName}`;
            content = `
                <div style="font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto; border: 1px solid #ffcdd2; padding: 25px; border-radius: 12px; box-shadow: 0 4px 10px rgba(0,0,0,0.05);">
                    <h2 style="color: #c62828; margin-top: 0; border-bottom: 2px solid #c62828; padding-bottom: 10px;">Smart Pillbox - Nhac Nho</h2>
                    <p style="font-size: 15px;">Hệ thống phát hiện lịch uống thuốc của bạn đã quá giờ hẹn nhưng chưa ghi nhận tín hiệu lấy thuốc từ thiết bị.</p>
                    <div style="background-color: #ffebee; padding: 15px; border-radius: 8px; margin: 20px 0; border-left: 4px solid #f44336;">
                        <p style="margin: 5px 0;"><b>Ten thuoc:</b> <span style="font-size: 16px; color: #b71c1c; font-weight: bold;">${medicineName}</span></p>
                        ${conditionText}
                        <p style="margin: 5px 0;"><b>Cu uong quy dinh:</b> ${formattedTime}</p>
                    </div>
                    <p style="color: #c62828; font-weight: bold; font-size: 14px;">Vui lòng tiến hành uống thuốc đúng chỉ định càng sớm càng tốt để đảm bảo hiệu quả điều trị tối ưu!</p>
                </div>
            `;
        }

        return await sendMail(to, subject, content);
    }
};