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
    if (self.activatingPromiseResolve) {
        self.activatingPromiseResolve();
        self.activatingPromiseResolve = null;
    }
};

onactivate = (event) => {
    event.waitUntil(new Promise(resolve => self.activatingPromiseResolve = resolve));
    self.port.postMessage("activating");
}

self.addEventListener('fetch', async (event) => {
    if (event.request.url.includes("no-fetch-event-handling"))
        return;

    if (event.request.url.includes("useNavigationPreloadPromise") && event.preloadResponse) {
        event.respondWith(event.preloadResponse);
        return;
    }

    if (event.request.url.includes("getResponseFromNavigationPreload") && event.preloadResponse) {
        event.respondWith((async () => {
            const response = await event.preloadResponse;
            if (response)
                 return response;
            return fetch(event.request);
        })());
        return;
    }

    if (event.request.url.includes("getResponseFromRequestWithCustomHeader")) {
        const newRequest = new Request(event.request, {
            headers: { "x-custom-header": "my-custom-header" },
        });
        event.respondWith(fetch(newRequest));
        return;
    }

    if (event.request.url.includes("getCloneResponseFromNavigationPreload") && event.preloadResponse) {
        event.respondWith((async () => {
            const response = await event.preloadResponse;
            if (response)
                 return response.clone();
            return fetch(event.request);
        })());
        return;
    }

    event.respondWith(fetch(event.request));
});
