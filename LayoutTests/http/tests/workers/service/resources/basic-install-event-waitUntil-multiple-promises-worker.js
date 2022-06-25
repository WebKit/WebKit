let resolvedPromiseCount = 0;

self.addEventListener("install", (event) => {
    for (let i = 0; i < 10; i++) {
        event.waitUntil(new Promise((resolve, reject) => {
            setTimeout(() => {
                resolvedPromiseCount++;
                resolve();
            }, 50 + i);
        }));
    }
});

self.addEventListener("message", (event) => {
    event.source.postMessage(resolvedPromiseCount);
});

