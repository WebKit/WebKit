let responseToSend = { status: 404, statusText: "Not Found" };

self.addEventListener("message", (event) => {
    responseToSend = event.data;
    responseToSend.statusText += " messageEvent.origin was " + event.origin;
});

self.addEventListener("fetch", (event) => {
    if (event.request.url.indexOf("test1") !== -1) {
        event.respondWith(new Response(null, responseToSend));
        return;
    }
    event.respondWith(Response.error());
});
