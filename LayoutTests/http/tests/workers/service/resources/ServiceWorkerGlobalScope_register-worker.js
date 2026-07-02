let client = null;

function log(msg)
{
    client.postMessage(msg);
}

async function runTest()
{
    try {
        let r = await navigator.serviceWorker.register("empty-worker.js", { scope: "/workers/service/resources/test", updateViaCache: "none" });
        log("PASS: Registration succeeded");

        if (r.scope == "http://127.0.0.1:8000/workers/service/resources/test")
            log("PASS: registration object's scope is valid");
        else
            log("FAIL: registration object's scope is invalid, got: " + r.scope);

        if (r.updateViaCache == "none")
            log("PASS: registration object's updateViaCache is valid");
        else
            log("FAIL: registration object's updateViaCache is invalid, got: " + r.updateViaCache);

        worker = r.installing;
        worker.addEventListener("statechange", function() {
            if (worker.state === "activated") {
                log("PASS: service worker is now active");
                log("DONE");
            }
        });
    } catch (e) {
        log("FAIL: " + e);
        log("DONE");
    }
}

self.addEventListener("message", (event) => {
    client = event.source;
    runTest();
});
