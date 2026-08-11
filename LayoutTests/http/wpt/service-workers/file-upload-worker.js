addEventListener("fetch", async (e) => {
    if (e.request.url.includes("do-not-handle"))
        return;
    e.respondWith(fetch(e.request));
});
