self.addEventListener("fetch", (event) => {
    if (!event.request.url.includes("download-task-uaf"))
        return;
    const stream = new ReadableStream({
        start: (controller) => {
            controller.enqueue(new TextEncoder().encode("chunk"));
            // Keep the stream open; the test will overwrite the SW context
            // connection and burst DidFinish before this stream closes.
        }
    });
    event.respondWith(new Response(stream, {
        "headers": [
            ["Content-Type", "application/binary"],
            ["Content-Disposition", "attachment"]
        ]
    }));
});
