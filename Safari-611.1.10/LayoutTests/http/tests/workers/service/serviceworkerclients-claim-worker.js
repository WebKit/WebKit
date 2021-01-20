self.addEventListener("fetch", async (event) => {
    if (event.request.url.indexOf("pinkelephant") !== -1)
        event.respondWith(new Response("PASS"));
});

var tryClaimPromise = self.clients.claim().then(() => {
    return "FAIL: claim did not throw";
}, (e) => {
    return "PASS, received exception " + e;
});

async function doTest(event)
{
    if (event.data !== "start") {
        event.source.postMessage("FAIL: received unexpected message from client");
        return;
    }

    var result = await tryClaimPromise;
    if (!result.startsWith("PASS")) {
        event.source.postMessage(result);
        return;
    }

    self.clients.claim().then(() => {
        event.source.postMessage("CLAIMED");
    }, () => {
        event.source.postMessage("FAIL: received exception " + e);
    });
}


self.addEventListener("message", doTest);

