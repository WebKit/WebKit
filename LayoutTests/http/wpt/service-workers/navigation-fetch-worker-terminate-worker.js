function doTest(event)
{
    event.respondWith(new Response("<html><body>PASS</body></html>", { status: 200, headers: [["Content-Type", "text/html"]] }));
    if (self.internals)
        setTimeout(() => internals.terminate(), 0);
}

self.addEventListener("fetch", doTest);
