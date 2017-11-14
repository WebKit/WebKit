function done()
{
    finishSWTest();
}

async function test()
{
    if (!await internals.hasServiceWorkerRegistration(self.origin))
        log("PASS: No service worker is initially registered for this origin");
    else
        log("FAIL: A service worker is initially registered for this origin");

    testRunner.setPrivateBrowsingEnabled(true);

    if (!await internals.hasServiceWorkerRegistration(self.origin))
        log("PASS: No service worker is initially registered for this origin in private session");
    else
        log("FAIL: A service worker is initially registered for this origin in private session");

    testRunner.setPrivateBrowsingEnabled(false);
    try {
        r = await navigator.serviceWorker.register("resources/empty-worker.js", { scope: "/test", updateViaCache: "none" })
        log("Registered!");

        if (r.scope == "http://127.0.0.1:8000/test")
            log("PASS: registration object's scope is valid");
        else
            log("FAIL: registration object's scope is invalid, got: " + r.scope);

        if (r.updateViaCache == "none")
            log("PASS: registration object's updateViaCache is valid");
        else
            log("FAIL: registration object's updateViaCache is invalid, got: " + r.updateViaCache);

        if (await internals.hasServiceWorkerRegistration("/test"))
            log("PASS: A service worker is now registered for this origin");
        else
            log("FAIL: No service worker is registered for this origin");

        testRunner.setPrivateBrowsingEnabled(true);

        if (!await internals.hasServiceWorkerRegistration("/test"))
            log("PASS: No service worker is registered for this origin in private session");
        else
            log("FAIL: A service worker is registered for this origin in private session");

        testRunner.setPrivateBrowsingEnabled(false);

        r = await navigator.serviceWorker.register("resources/empty-worker-doesnt-exist.js", { })
        log("Registered!");
    } catch (e) {
        log("Exception registering: " + e);
    }
    done();
}

test();
