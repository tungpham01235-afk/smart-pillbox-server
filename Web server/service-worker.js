// ===================================================
// Smart Pillbox - Service Worker v2.0
// Chiến lược: Stale-While-Revalidate + Offline Fallback
// ===================================================

const CACHE_VERSION = 'v2';
const CACHE_NAME = `smart-pillbox-${CACHE_VERSION}`;
const OFFLINE_URL = '/offline.html';

// Danh sách tài nguyên tĩnh cần cache ngay khi cài đặt
const PRECACHE_ASSETS = [
    '/',
    '/index.html',
    '/dashboard.html',
    '/offline.html',
    '/manifest.json',
    '/icon.png',
    '/icon-512.png'
];

// ===================================================
// 1. CÀI ĐẶT: Pre-cache tài nguyên tĩnh
// ===================================================
self.addEventListener('install', (event) => {
    console.log(`[SW ${CACHE_VERSION}] Đang cài đặt và pre-caching assets...`);
    event.waitUntil(
        caches.open(CACHE_NAME)
            .then((cache) => {
                // Dùng addAll để đảm bảo tất cả assets được cache
                return cache.addAll(PRECACHE_ASSETS);
            })
            .then(() => {
                console.log(`[SW ${CACHE_VERSION}] Pre-cache hoàn tất!`);
                // Kích hoạt ngay lập tức mà không cần chờ tab cũ đóng
                return self.skipWaiting();
            })
            .catch(err => console.error('[SW] Lỗi pre-cache:', err))
    );
});

// ===================================================
// 2. KÍCH HOẠT: Xóa cache phiên bản cũ
// ===================================================
self.addEventListener('activate', (event) => {
    console.log(`[SW ${CACHE_VERSION}] Kích hoạt, đang dọn cache cũ...`);
    event.waitUntil(
        caches.keys()
            .then((cacheNames) => {
                return Promise.all(
                    cacheNames
                        .filter(name => name.startsWith('smart-pillbox-') && name !== CACHE_NAME)
                        .map(name => {
                            console.log(`[SW] Xóa cache cũ: ${name}`);
                            return caches.delete(name);
                        })
                );
            })
            .then(() => self.clients.claim()) // Tiếp quản tất cả tabs mở sẵn
    );
});

// ===================================================
// 3. XỬ LÝ FETCH: Stale-While-Revalidate + Offline Fallback
// ===================================================
self.addEventListener('fetch', (event) => {
    const { request } = event;
    const url = new URL(request.url);

    // [A] Bỏ qua hoàn toàn: API calls & Socket.io (luôn cần dữ liệu thật)
    if (url.pathname.startsWith('/api/') || url.pathname.startsWith('/socket.io/')) {
        return;
    }

    // [B] Bỏ qua: Cross-origin requests (CDN, Google Fonts, v.v.)
    if (url.origin !== self.location.origin) {
        return;
    }

    // [C] Chiến lược Stale-While-Revalidate cho tài nguyên tĩnh
    event.respondWith(
        caches.open(CACHE_NAME).then(async (cache) => {
            const cachedResponse = await cache.match(request);

            // Fetch mạng ngầm để cập nhật cache (không chặn response)
            const networkFetchPromise = fetch(request)
                .then((networkResponse) => {
                    if (networkResponse && networkResponse.status === 200) {
                        // Chỉ cache GET requests với nội dung hợp lệ
                        if (request.method === 'GET') {
                            cache.put(request, networkResponse.clone());
                        }
                    }
                    return networkResponse;
                })
                .catch(() => null); // Mạng thất bại, không làm gì thêm

            if (cachedResponse) {
                // Có cache → trả về ngay, fetch mạng ngầm để cập nhật cache
                networkFetchPromise.catch(() => {}); // đảm bảo khởi động và bắt lỗi im lặng
                return cachedResponse;
            }

            // Không có cache → đợi mạng
            const networkResponse = await networkFetchPromise;
            if (networkResponse) {
                return networkResponse;
            }

            // Không có cả cache lẫn mạng → Offline fallback
            console.warn('[SW] Offline - Trả về trang offline.html');
            const offlinePage = await caches.match(OFFLINE_URL);
            return offlinePage || new Response('Offline', { status: 503 });
        })
    );
});

// ===================================================
// 4. XỬ LÝ PUSH NOTIFICATIONS (Web Push)
// ===================================================
self.addEventListener('push', (event) => {
    let data = {};
    try {
        data = event.data ? event.data.json() : {};
    } catch (e) {
        data = { title: 'Smart Pillbox', body: event.data ? event.data.text() : 'Thông báo mới!' };
    }

    const title = data.title || '💊 Smart Pillbox';
    const options = {
        body: data.body || 'Bạn có thông báo mới từ tủ thuốc.',
        icon: '/icon-512.png',
        badge: '/icon.png',
        vibrate: [200, 100, 200],
        tag: data.tag || 'pillbox-notification',
        data: {
            url: data.url || '/dashboard.html',
            dateOfArrival: Date.now()
        },
        actions: [
            { action: 'open', title: '📱 Mở App', icon: '/icon.png' },
            { action: 'dismiss', title: '✖ Bỏ qua' }
        ],
        requireInteraction: data.requireInteraction || false
    };

    event.waitUntil(
        self.registration.showNotification(title, options)
    );
});

// ===================================================
// 5. XỬ LÝ CLICK NOTIFICATION
// ===================================================
self.addEventListener('notificationclick', (event) => {
    event.notification.close();

    const targetUrl = (event.notification.data && event.notification.data.url) 
        ? event.notification.data.url 
        : '/dashboard.html';

    if (event.action === 'dismiss') return;

    event.waitUntil(
        clients.matchAll({ type: 'window', includeUncontrolled: true })
            .then((clientList) => {
                // Nếu app đang mở, focus vào đó
                for (const client of clientList) {
                    if (client.url.includes(self.location.origin) && 'focus' in client) {
                        client.navigate(targetUrl);
                        return client.focus();
                    }
                }
                // Không có app mở → mở tab mới
                if (clients.openWindow) {
                    return clients.openWindow(targetUrl);
                }
            })
    );
});

// ===================================================
// 6. BACKGROUND SYNC (Đồng bộ khi có mạng trở lại)
// ===================================================
self.addEventListener('sync', (event) => {
    if (event.tag === 'sync-pill-logs') {
        console.log('[SW] Background sync: Đang đồng bộ logs chưa gửi...');
        event.waitUntil(syncPendingLogs());
    }
});

async function syncPendingLogs() {
    try {
        // Đọc logs đang chờ từ IndexedDB hoặc localStorage
        const clients_list = await clients.matchAll();
        clients_list.forEach(client => {
            client.postMessage({ type: 'SYNC_LOGS_REQUEST' });
        });
    } catch (e) {
        console.error('[SW] Lỗi sync logs:', e);
    }
}