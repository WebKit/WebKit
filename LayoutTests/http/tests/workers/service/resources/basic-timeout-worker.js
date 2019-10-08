self.addEventListener("fetch", (event) => {
    if (event.request.url.indexOf("timeout") !== -1) {
        while (1) { }
    } else
        event.respondWith(new Response(null, { status: 200, statusText: "Hello from service worker", headers: [["Source", "ServiceWorker"]] }));
});
