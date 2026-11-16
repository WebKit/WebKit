self.addEventListener("fetch", (event) => {
    if (event.request.url.includes("download-body")) {
        const response = new Response("a download body", {"headers" : [["Content-Type", "application/binary"]]});
        event.respondWith(response);
        return;
    }
    if (event.request.url.includes("download-stream")) {
        const stream = new ReadableStream({
            start: (controller) => {
                const encoder = new TextEncoder();
                controller.enqueue(encoder.encode("a first chunk, "));
                setTimeout(() => { controller.enqueue(encoder.encode("then a second chunk")); }, 100);
                setTimeout(() => { controller.close(); }, 200);
            }
        });
        const response = new Response(stream, {"headers" : [["Content-Type", "application/binary"], ["Content-Length", 200]]});
        event.respondWith(response);
        return;
    }
    event.respondWith(fetch(event.request.url));
});
