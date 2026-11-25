self.addEventListener('activate', event => {
  event.waitUntil(self.registration.navigationPreload.enable());
});

self.addEventListener('fetch', event => {
  event.respondWith(new Response('<!DOCTYPE html><body>synthetic',
      {headers: {'Content-Type': 'text/html'}}));
  event.waitUntil((async () => {
    let result;
    try {
      const response = await event.preloadResponse;
      result = response ? await response.text() : 'no preloadResponse';
    } catch (e) {
      result = `${e.name}: ${e.message}`;
    }
    const clients = await self.clients.matchAll({includeUncontrolled: true});
    for (const client of clients)
      client.postMessage({url: event.request.url, result});
  })());
});
