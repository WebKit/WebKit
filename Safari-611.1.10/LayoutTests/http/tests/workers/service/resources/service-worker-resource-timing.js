self.addEventListener("install", (event) => {
    event.waitUntil(
        caches.open("resource-timing-cache").then((cache) => {
            return Promise.all([
                cache.addAll(["/workers/service/resources/data1.txt"]),
                fetch("/workers/service/resources/data2.txt"),
            ]);
        })
    );
});

self.addEventListener("message", (event) => {
    event.source.postMessage({entries: JSON.stringify(performance.getEntries())});
});
