const CACHE_NAME = 'elektron-app-v1';
const urlsToCache = [
  './',
  './index.html',
  './login.html',
  './icon.png'
];

// Install Service Worker
self.addEventListener('install', event => {
  event.waitUntil(
    caches.open(CACHE_NAME).then(cache => {
      return cache.addAll(urlsToCache);
    })
  );
});

// Fetch API (Bypass Firebase agar sensor tetap Real-time)
self.addEventListener('fetch', event => {
  if (event.request.url.includes('firebase') || event.request.url.includes('google')) {
    return; // Jangan cache data sensor, biarkan live!
  }
  event.respondWith(
    caches.match(event.request).then(response => {
      return response || fetch(event.request);
    })
  );
});