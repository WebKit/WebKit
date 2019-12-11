var source;
addEventListener("message", (e) => {
    source = e.source;
    source.postMessage(e.data === "ready?" ? "ready" : "not ready");
});

addEventListener("fetch", async (e) => {
    var promise = fetch(e.request);
    e.respondWith(promise.then((response) => {
        if (self.internals)
            source.postMessage(internals.fetchResponseHeaderList(response).sort());
        else
            source.postMessage("Test requires internals API to get all response headers");
        return response;
    }));
});
