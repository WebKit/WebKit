self.addEventListener('install', (event) => {
    event.waitUntil(caches.open("cache_name").then((cache) => {
        return cache.addAll(['/WebKit/service-workers/resources/navigation-timing-part-2.html']);
    }));
});

self.addEventListener('fetch', (event) => {
    setTimeout(()=>{
        event.respondWith(caches.open("cache_name").then((cache) => {
            return cache.match(event.request).then((response) => {
                if (!response)
                    return fetch(event.request);
                return response;
            });
        }));
    }, 500);
});
