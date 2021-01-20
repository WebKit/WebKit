self.addEventListener("fetch", (event) => {
    if (event.request.url.includes("test-200")) {
        event.respondWith(new Response(null, {status: 200, statusText: "Custom Status Text: OK"}));
        return;
    }

    if (event.request.url.includes("test-500")) {
        event.respondWith(new Response(null, {status: 500, statusText: "Custom Status Text: Error"}));
        return;
    }

    if (event.request.url.includes("error")) {
        event.respondWith(Response.error());
        return;
    }

    event.respondWith(fetch(event.request.url));
});
