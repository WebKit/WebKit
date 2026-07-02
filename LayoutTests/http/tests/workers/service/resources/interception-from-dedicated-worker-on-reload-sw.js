self.addEventListener("fetch", (event) => {
    if (event.request.url.indexOf("foo.txt") !== -1) {
        event.respondWith(new Response("INTERCEPTED", { status: 200, statusText: "Hello from service worker", headers: [["Hello", "World"]] }));
        return;
    }
});
