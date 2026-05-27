self.addEventListener("activate", (event) => {
    event.waitUntil(clients.claim());
});

self.addEventListener("fetch", (event) => {
    if (event.request.url.indexOf("test1") >= 0) {
        event.respondWith(new Promise((resolve) => {
            navigator.locks.request('abc', {mode: 'shared'}, () => {
                resolve(new Response("PASS"));
            }).catch((error) => {
                resolve(new Response(`FAIL - ${error}`));
            });
        }));
        return;
    }
    if (event.request.mode !== "navigate") {
        event.respondWith(Response.error());
        return;
    }
    event.respondWith(fetch(event.request.url));
});
