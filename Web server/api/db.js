'use strict';
const mongoose = require('mongoose');
const dns = require('dns');

// Ép tiến trình Node.js sử dụng DNS của Google để phân giải SRV của MongoDB Atlas
try {
    dns.setServers(['8.8.8.8', '8.8.4.4']);
    console.log('🌐 [DNS] Đã cấu hình DNS Google làm máy chủ phân giải chính.');
} catch (err) {
    console.warn('⚠️ [DNS] Lỗi cấu hình DNS nội bộ:', err.message);
}

// Kích hoạt chế độ debug để theo dõi câu lệnh truy vấn
mongoose.set('debug', true);

// Kết nối tới MongoDB
mongoose.connect(process.env.MONGODB_URI)
  .then(() => {
      console.log('✅ Kết nối MongoDB thành công!');
  })
  .catch((err) => {
      console.error('❌ Lỗi kết nối database:', err.message);
      // Dừng ứng dụng nếu không kết nối được DB (tránh lỗi ngầm)
      process.exit(1); 
  });

// Lắng nghe các sự kiện thay đổi trạng thái kết nối
mongoose.connection.on('error', (err) => {
    console.error('❌ MongoDB Connection Error:', err);
});

mongoose.connection.on('disconnected', () => {
    console.warn('⚠️ Mất kết nối tới MongoDB. Đang thử kết nối lại...');
});

module.exports = mongoose;