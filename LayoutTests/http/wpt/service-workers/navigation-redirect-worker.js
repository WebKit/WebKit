addEventListener("fetch", async (e) => {
    if (e.request.url.indexOf("navigation-redirect-frame1.html") !== -1) {
        //e.respondWith(new Response(self.registration.scope));
        e.respondWith(Response.redirect("resources/navigation-redirect-frame2.html"));
        return;
    }
    if (e.request.url.indexOf("navigation-redirect-frame2.html") !== -1) {
        e.respondWith(new Response(self.registration.scope));
        return;
    }
});
