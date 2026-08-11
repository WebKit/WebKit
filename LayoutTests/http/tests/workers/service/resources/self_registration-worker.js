let results = [];

function log(msg)
{
    results.push(msg);
}

function dumpServiceWorker(workerName, serviceWorker)
{
    if (!serviceWorker) {
        log(workerName + " worker: null");
        return;
    }
    log(workerName + " worker:");
    log("- scriptURL: " + serviceWorker.scriptURL);
    log("- state: " + serviceWorker.state);
}

function dumpRegistration()
{
    log("* self.registration");
    log("scope: " + self.registration.scope);
    log("updateViaCache: " + self.registration.updateViaCache);
    dumpServiceWorker("installing", self.registration.installing);
    dumpServiceWorker("waiting", self.registration.waiting);
    dumpServiceWorker("active", self.registration.active);
    log("");
}

self.addEventListener("install", function() {
    log("Received install event");
    dumpRegistration();

    if (self.registration.installing) {
        self.registration.installing.addEventListener("statechange", function() {
            log("Received statechange event on service worker");
        })
    }
});

self.addEventListener("activate", function() {
    log("Received activate event");
    dumpRegistration();
});

self.addEventListener("message", (event) => {
    dumpRegistration();
    for (let result of results)
        event.source.postMessage(result);
    event.source.postMessage("DONE");
});

self.registration.addEventListener("updatefound", function() {
    log("Received updatefound event on self.registration");
});
