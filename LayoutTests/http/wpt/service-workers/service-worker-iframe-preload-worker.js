oninstall = (event) => {
    if (self.port)
        return;
    event.waitUntil(new Promise(resolve => self.installingPromiseResolve = resolve));
}

onmessage = (event) => {
    if (event.data.port)
        self.port = event.data.port;
    if (self.installingPromiseResolve) {
        self.installingPromiseResolve();
        self.installingPromiseResolve = null;
    }
    if (self.fetchingPromiseResolve) {
        self.fetchingPromiseResolve();
        self.fetchingPromiseResolve = null;
    }
};

onactivate = (event) => {
    self.port.postMessage("activating");
}

self.addEventListener('fetch', (event) => {
    self.port.postMessage("fetching");
    const promise = new Promise(resolve => self.fetchingPromiseResolve = resolve);
    event.respondWith(promise.then(async () => {
        if (!event.preloadResponse)
            return new Response("FAIL: preload not enabled");
        const response = await event.preloadResponse;
        if (!response)
            return new Response("FAIL: no preload response");
        return response;
    }));
});
