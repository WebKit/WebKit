function doTest(event)
{
    event.respondWith(new Response('<html><body>Test</body></html>', { status: 200, statusText: "Hello from service worker", headers: [["Content-Type", "text/html; charset=utf-8"]] }));
}

self.addEventListener("fetch", doTest);
