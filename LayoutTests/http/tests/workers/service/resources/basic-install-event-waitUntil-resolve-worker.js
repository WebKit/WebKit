let resolvedExtendLifetimePromise = false;

self.addEventListener("install", (event) => {
    event.waitUntil(new Promise((resolve, reject) => {
        setTimeout(() => {
            resolvedExtendLifetimePromise = true;
            resolve();
        }, 50);
    }));
});

self.addEventListener("message", (event) => {
    event.source.postMessage(resolvedExtendLifetimePromise);
});

