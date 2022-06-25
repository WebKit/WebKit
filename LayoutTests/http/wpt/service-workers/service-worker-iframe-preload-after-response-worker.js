self.addEventListener('fetch', async (event) => {
    let stream = new TransformStream();
    let resolve;
    let promise = new Promise(r => resolve = r);
    event.waitUntil(promise);
    event.respondWith(new Response(stream.readable, { status: 200, headers: [["ContentType", "text/html"]] }));

    await new Promise(resolve => setTimeout(resolve, 50));

    const response = await event.preloadResponse;
    response.body.pipeTo(stream.writable);
    resolve();
});
