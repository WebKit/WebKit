addEventListener('activate', function(event) {
    event.waitUntil(self.clients.claim());
});

addEventListener("message", (e) => {
    self.port = e.data;
    self.port.postMessage("ok");
});

addEventListener("fetch", async (e) => {
    if (e.request.url.indexOf("image1") !== -1) {
        if (self.port)
             self.port.postMessage({url: e.request.url, destination: e.request.destination});
        const blob = new Blob(["<svg xmlns='http://www.w3.org/2000/svg' version='1.0'><text x='20' y='20' font-size='20' fill='black'>OK</text></svg>"], { "type" : "image/svg+xml" });
        e.respondWith(new Response(blob));
        return;
    }
});
