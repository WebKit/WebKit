let client = null;

function log(msg)
{
    client.postMessage(msg);
}

async function runTest()
{
    try {
        let r = await navigator.serviceWorker.register("empty-worker.js", { scope: "/test" });
        log("PASS: Registration succeeded");

        let retrievedRegistration = await navigator.serviceWorker.getRegistration("/test");
        if (r === retrievedRegistration)
            log("PASS: getRegistration() returned the right registration");
        else
            log("FAIL: getRegistration() did not return the right registration");

        let retrievedRegistrations = await navigator.serviceWorker.getRegistrations();
        if (retrievedRegistrations.length === 2)
            log("PASS: getRegistrations() returned 2 registrations");
        else {
            log("FAIL: getRegistrations() returned " + retrievedRegistrations.length + " registration(s)");
            log("DONE");
            return;
        }

        if (retrievedRegistrations[0] === self.registration)
            log("PASS: getRegistrations()[0] is the right registration");
        else
            log("FAIL: getRegistrations()[0] is not the right registration");

        if (retrievedRegistrations[1] === r)
            log("PASS: getRegistrations()[1] is the right registration");
        else
            log("FAIL: getRegistrations()[1] is not the right registration");

        log("DONE");
    } catch (e) {
        log("FAIL: " + e);
        log("DONE");
    }
}

self.addEventListener("message", (event) => {
    client = event.source;
    runTest();
});
