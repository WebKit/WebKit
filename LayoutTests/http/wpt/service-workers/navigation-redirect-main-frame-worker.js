addEventListener("fetch", async (e) => {
    e.respondWith(Response.redirect("/WebKit/service-workers/navigation-redirect-main-frame.https.html#redirected"));
});
