addEventListener("message", async (e) => {
    if (!e.data.port)
        return;

    self.port = e.data.port;
    self.port.postMessage("ok");
});

addEventListener("fetch", async (e) => {
    if (e.request.url.includes("clone")) {
        const cloned = e.request.clone();
        e.respondWith(fetch(e.request.clone()));
        if (self.port)
            self.port.postMessage(await cloned.text());
        return;
    }
    e.respondWith(fetch(e.request));
});
