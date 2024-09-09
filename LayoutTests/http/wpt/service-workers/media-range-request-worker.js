self.onactivate = event => {
    event.waitUntil(self.clients.claim());
}

var videoRequestCount = 0;
self.onfetch = event => {
    if (event.request.url.includes("clearVideoRequestCount")) {
        videoRequestCount = 0;
        event.respondWith(new Response("OK"));
        return;
    }
    if (!event.request.url.includes("test.mp4"))
        return;
    if (!videoRequestCount++) {
        const request = new Request("http://localhost:8800/WebKit/resources/test.mp4", { headers: {
            "Range": "bytes=0-1",
        }});
        event.respondWith(fetch(request));
        event.respondWith(new Response(new Uint8Array([0]),
            {status: 206, headers: {
                "Content-Type": "video/mp4",
                "Content-Range": `bytes 0-1/192844`
            }}
        ));
        return;
    }
    // If video player is changing origin based on the previous request, we are safe and can trigger the video load to fail to pass the test.
    if (event.request.url.includes("http://localhost:8800"))
        return event.respondWith(new Response("", { status: 400, statusText: "failing video as URL was rewritten by player" }));
}
