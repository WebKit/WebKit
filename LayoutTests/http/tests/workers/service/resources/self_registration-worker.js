let results = [];

function log(msg)
{
    results.push(msg);
}

function dumpRegistration()
{
    log("* self.registration");
    log("scope: " + self.registration.scope);
    log("updateViaCache: " + self.registration.updateViaCache);
    log("");
}

self.addEventListener("install", function() {
    log("Received install event");
    dumpRegistration();
});

self.addEventListener("message", (event) => {
    for (let result of results)
        event.source.postMessage(result);
    event.source.postMessage("DONE");
});

