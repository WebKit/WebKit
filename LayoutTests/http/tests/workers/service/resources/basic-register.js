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

navigator.serviceWorker.register("resources/empty-worker.js", { })
.then(function(r) {
	log("Registered!");

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
