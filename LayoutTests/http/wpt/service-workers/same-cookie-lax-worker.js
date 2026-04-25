async function doTest(event)
{
    try {
        if (!event.data.startsWith("test-cookie")) {
            event.source.postMessage("FAIL: received unexpected message from client");
            return;
        }

        const response = await fetch("resources/get-cookie.py");
        event.source.postMessage(await response.text());
    } catch (e) {
        event.source.postMessage(e);
    }
}

function doFetch(event)
{
    if (event.request.url.includes("fetchTest"))
        event.respondWith(fetch(event.request));
}

self.addEventListener("message", doTest);
self.addEventListener("fetch", doFetch);
