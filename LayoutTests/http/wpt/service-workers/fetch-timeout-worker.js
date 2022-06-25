async function doTest(event)
{
    event.respondWith(fetch(event.request));
}

self.addEventListener("fetch", doTest);
