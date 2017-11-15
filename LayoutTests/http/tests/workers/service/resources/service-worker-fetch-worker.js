var status = "no status";
self.addEventListener("fetch", (event) => {
    if (event.request.url.indexOf("status") !== -1) {
        event.respondWith(new Response(null, {status: 200, statusText: status}));
        return;
    }
    if (event.request.url.endsWith(".fromserviceworker")) {
        status = event.request.url.substring(0, event.request.url.length - 18) + " through " + "fetch";
        event.respondWith(fetch(event.request.url.substring(0, event.request.url.length - 18)));
    }
    if (event.request.url.endsWith(".bodyasanemptystream")) {
        var stream = new ReadableStream({ start : controller => {
            controller.close();
        }});
        event.respondWith(new Response(stream, {status : 200, statusText : "Empty stream"}));
        return;
    }
    state = "unknown url";
    event.respondWith(new Response(null, {status: 404, statusText: "Not Found"}));
    return;
});
