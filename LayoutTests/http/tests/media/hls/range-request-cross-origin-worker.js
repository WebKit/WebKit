self.onactivate = event => {
    event.waitUntil(self.clients.claim());
}

self.onfetch = event => {
    event.respondWith(fetch(event.request));
}
