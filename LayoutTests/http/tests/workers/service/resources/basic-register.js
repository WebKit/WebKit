function done()
{
    finishSWTest();
}

function log(msg)
{
    let console = document.getElementById("console");
    if (!console) {
        console = document.createElement("div");
        console.id = "console";
        document.body.appendChild(console);
    }
    let span = document.createElement("span");
    span.innerHTML = msg + "<br>";
    console.appendChild(span);
}

if (!internals.hasServiceWorkerRegisteredForOrigin(self.origin))
    log("PASS: No service worker is initially registered for this origin");
else
    log("FAIL: A service worker is initially registered for this origin");

testRunner.setPrivateBrowsingEnabled(true);

if (!internals.hasServiceWorkerRegisteredForOrigin(self.origin))
    log("PASS: No service worker is initially registered for this origin in private session");
else
    log("FAIL: A service worker is initially registered for this origin in private session");

testRunner.setPrivateBrowsingEnabled(false);

navigator.serviceWorker.register("resources/empty-worker.js", { scope: "/test", updateViaCache: "none" })
.then(function(r) {
	log("Registered!");

        if (r.scope == "http://127.0.0.1:8000/test")
           log("PASS: registration object's scope is valid");
        else
           log("FAIL: registration object's scope is invalid, got: " + r.scope);

        if (r.updateViaCache == "none")
           log("PASS: registration object's updateViaCache is valid");
        else
           log("FAIL: registration object's updateViaCache is invalid, got: " + r.updateViaCache);

        if (internals.hasServiceWorkerRegisteredForOrigin(self.origin))
            log("PASS: A service worker is now registered for this origin");
        else
            log("FAIL: No service worker is registered for this origin");

        testRunner.setPrivateBrowsingEnabled(true);

        if (!internals.hasServiceWorkerRegisteredForOrigin(self.origin))
            log("PASS: No service worker is registered for this origin in private session");
        else
            log("FAIL: A service worker is registered for this origin in private session");

        testRunner.setPrivateBrowsingEnabled(false);
}, function(e) {
        log("Registration failed with error: " + e);
})
.catch(function(e) {
	log("Exception registering: " + e);
});

navigator.serviceWorker.register("resources/empty-worker-doesnt-exist.js", { })
.then(function(r) {
	log("Registered!");
	done();
}, function(e) {
	log("Registration failed with error: " + e);
	done();
})
.catch(function(e) {
	log("Exception registering: " + e);
	done();
});
