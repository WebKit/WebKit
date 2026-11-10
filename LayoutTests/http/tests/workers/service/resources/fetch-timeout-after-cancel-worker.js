self.addEventListener("fetch", (event) => {
    event.respondWith(new Response("intercepted", { headers: { "Content-Type": "text/html" } }));
});
