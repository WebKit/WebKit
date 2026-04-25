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
