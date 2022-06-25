function doTest(event)
{
    if (event.request.url.includes('?intercept'))
        event.respondWith(fetch(event.request));
}

self.addEventListener("fetch", doTest);
