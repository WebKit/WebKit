var response;
var status = "no status";
self.addEventListener("fetch", (event) => {
    if (event.request.url.indexOf("status") !== -1) {
        event.respondWith(new Response(null, {status: 200, statusText: status}));
        return;
    }
    if (!event.request.url.endsWith(".fromserviceworker")) {
        status = "unknown url";
        event.respondWith(new Response(null, {status: 404, statusText: "Not Found"}));
        return;
    }
    status = "Fetching " + event.request.url.substring(0, event.request.url.length - 18);
    event.respondWith(fetch(event.request.url.substring(0, event.request.url.length - 18)).then((r) => {
        response = r;
        status = "Got response for " + event.request.url.substring(0, event.request.url.length - 18) + ", status code is " + response.status;
        return response.arrayBuffer();
    }).then((buffer) => {
        var headers = new Headers(response.headers);
        headers.set("cache-control", "no-cache");
        return new Response(buffer, {headers: headers});
    }));
});
