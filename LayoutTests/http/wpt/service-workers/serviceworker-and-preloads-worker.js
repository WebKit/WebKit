var fetches = [];

self.addEventListener("message", (e) => {
    if (e.data == "clear urls")
        fetches = [];
    e.source.postMessage(fetches);
});

self.addEventListener("fetch", (e) => {
    fetches.push(e.request.url);
    e.respondWidth(fetch(e.request));
});
