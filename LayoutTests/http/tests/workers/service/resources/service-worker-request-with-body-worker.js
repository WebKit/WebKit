self.addEventListener("fetch", (event) => {
    if (event.request.url.endsWith("request-with-text-body.fromserviceworker")) {
        event.respondWith(
            event.request.text().then(value => {
                return new Response(value);
            })
        );
        return;
    }
    if (event.request.url.endsWith("request-with-cloned-text-body.fromserviceworker")) {
        event.respondWith(
            event.request.clone().text().then(value => {
                return new Response(value);
            })
        );
    }
    if (event.request.url.endsWith("post-echo.cgi")) {
        event.respondWith(fetch(event.request));
        return;
    }
});
