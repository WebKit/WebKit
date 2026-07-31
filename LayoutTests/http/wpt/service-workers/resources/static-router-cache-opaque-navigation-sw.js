importScripts('/common/get-host-info.sub.js');

const remoteURL = get_host_info()['HTTPS_REMOTE_ORIGIN'] +
  '/WebKit/service-workers/resources/cache-opaque-cross-origin-secret.html';

self.addEventListener('install', event => {
  event.waitUntil((async () => {
    const cache = await caches.open('opaque-cache');
    const response = await fetch(remoteURL, { mode: 'no-cors' });
    await cache.put('opaque-route', response);
    await event.addRoutes([
      { condition: { urlPattern: '/**/opaque-route' }, source: 'cache' }
    ]);
  })());
  self.skipWaiting();
});

self.addEventListener('activate', event => {
  event.waitUntil(clients.claim());
});
