addEventListener("fetch", async (e) => {
    if (e.request.url.indexOf("module-meta-url.js") !== -1) {
        let blob = new Blob([`globalThis.metaURLs.push(import.meta.url);`], {
            type: "application/javascript"
        });
        e.respondWith(new Response(blob, {
            status: 200,
        }));
        return;
    }
});
