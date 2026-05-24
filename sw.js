const CACHE_NAME = 'elektron-app-v2'; // Naikkan versi agar cache lama otomatis terhapus
const urlsToCache = [
  './',
  './index.html',
  './dashboard.html',
  './login.html',
  './icon.png'
];

// Install & langsung aktifkan versi baru
self.addEventListener('install', event => {
  self.skipWaiting();
  event.waitUntil(
    caches.open(CACHE_NAME).then(cache => {
      return cache.addAll(urlsToCache);
    })
  );
});

// Hapus cache versi lama (Pembersih otomatis)
self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys().then(cacheNames => {
      return Promise.all(
        cacheNames.filter(cache => cache !== CACHE_NAME).map(cache => caches.delete(cache))
      );
    })
  );
});

// Network First Strategy (Utamakan internet biar Firebase lancar jaya)
self.addEventListener('fetch', event => {
  // Biarkan Firebase dan CDN memuat langsung dari internet
  if (event.request.url.includes('firestore') || event.request.url.includes('firebase') || event.request.url.includes('google')) {
    return;
  }
  
  event.respondWith(
    fetch(event.request).catch(() => {
      return caches.match(event.request);
    })
  );
});