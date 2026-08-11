async function doTest(event)
{
    if (event.preloadResponse) {
        event.respondWith(event.preloadResponse.then((response) => {
            if (event.request.url.includes("get-body")) {
                const clone = response.clone();
                clone.body.getReader();
                return response;
            }
            if (self.internals)
                setTimeout(() => internals.terminate(), 0);
            return response;
        }));
        return;
    }
    event.respondWith(fetch(event.request));
}

self.addEventListener("fetch", doTest);
