function done()
{
    finishSWTest();
}

async function test()
{
    try {
        if (!internals.hasServiceWorkerRegisteredForOrigin(self.origin))
            log("PASS: There is initially no service worker registered for the origin");
        else
            log("FAIL: There is initially a service worker registered for the origin");

        let registration = await navigator.serviceWorker.register("resources/basic-fetch-worker.js", { });
        if (registration.scope === "https://127.0.0.1:8443/workers/service/")
            log("PASS: registration scope is " + registration.scope);
        else
            log("FAIL: registration scope is " + registration.scope);
 
        if (internals.hasServiceWorkerRegisteredForOrigin(self.origin))
            log("PASS: There is a service worker registered for the origin");
        else
            log("FAIL: There is no service worker registered for the origin");

        let unregistrationResult = await registration.unregister();
        if (unregistrationResult)
            log("PASS: Unregistration was successful");
        else
            log("FAIL: Unregistration failed");

        if (!internals.hasServiceWorkerRegisteredForOrigin(self.origin))
            log("PASS: There is no service worker registered for the origin");
        else
            log("FAIL: There is a service worker registered for the origin");

        unregistrationResult = await registration.unregister();
        if (!unregistrationResult)
            log("PASS: Unregistration failed as expected");
        else
            log("FAIL: Unregistration succeeded unexpectedly");
        
        if (!internals.hasServiceWorkerRegisteredForOrigin(self.origin))
            log("PASS: There is no service worker registered for the origin");
        else
            log("FAIL: There is a service worker registered for the origin");

        registration = await navigator.serviceWorker.register("resources/basic-fetch-worker.js", { });
        if (registration.scope === "https://127.0.0.1:8443/workers/service/")
            log("PASS: registration scope is " + registration.scope);
        else
            log("FAIL: registration scope is " + registration.scope);

        if (internals.hasServiceWorkerRegisteredForOrigin(self.origin))
            log("PASS: There is a service worker registered for the origin");
        else
            log("FAIL: There is no service worker registered for the origin");
    } catch(e) {
        console.log("Got exception: " + e);
    }
    finishSWTest();
}

test();
