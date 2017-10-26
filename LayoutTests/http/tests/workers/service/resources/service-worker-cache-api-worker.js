var status = "no status";
var cache;
var worker = self;

async function prepareCache()
{
    status = "opening cache";
    cache = await caches.open("test");
    status = "creating response";
    var response = new Response(new ArrayBuffer(12940), { status: 200, statusText: "OK"});
    status = "filling cache";
    await cache.put("/resources/square100.png", response);
    status = "cache is ready";
}

function statusResponse()
{
    return new Response(null, {status: 200, statusText: status});
}

self.addEventListener("fetch", (event) => {
    if (event.request.url.indexOf("status") !== -1) {
        event.respondWith(promise.then(statusResponse, statusResponse));
        return;
    }
    if (!event.request.url.endsWith(".fromserviceworker")) {
        state = "unknown url";
        event.respondWith(new Response(null, {status: 404, statusText: "Not Found"}));
        return;
    }
    event.respondWith(promise.then(() => {
        status = "opening cache for " + event.request.url.substring(0, event.request.url.length - 18);
        return caches.open("test").then((cache) => {
            status = "matching cache for " + event.request.url.substring(0, event.request.url.length - 18);
            return cache.match(event.request.url.substring(0, event.request.url.length - 18));
        }).then((response) => {
            status = "matched cache for " + event.request.url.substring(0, event.request.url.length - 18);
            return response;
        });
    }));
});
var promise = prepareCache();
