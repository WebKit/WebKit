function doTest(event)
{
    if (!self.internals)
        return;

    if (event.request.url.includes("terminate-while-fetching")) {
        event.respondWith(new Promise(resolve => {
           // we do not resolve the promise.
           setTimeout(() => internals.terminate(), 500);
        }));
    }
}

self.addEventListener("fetch", doTest);
