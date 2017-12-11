let serviceWorkerHasReceivedState = false;
let worker = null;
let remainingAttempts = 1000; // We try for 10 seconds before timing out.

navigator.serviceWorker.addEventListener("message", function(event) {
    if (!serviceWorkerHasReceivedState) {
        if (!event.data) {
            log("FAIL: service worker did not receive the state");
            finishSWTest();
            return;
        }
        serviceWorkerHasReceivedState = true;

        log("* Simulating Service Worker process crash");
        testRunner.terminateServiceWorkerProcess();

        handle = setInterval(function() {
            remainingAttempts--;
            if (!remainingAttempts) {
                log("FAIL: service worker did not respond after process termination");
                clearInterval(handle);
                finishSWTest();
                return;
            }

            worker.postMessage("HasState");
        }, 10);
        return;
    }

    // Worker still has the state, it was not terminated yet.
    if (event.data === serviceWorkerHasReceivedState)
        return;

    log("PASS: service worker lost the state and responded the postMessage after process termination");
    clearInterval(handle);
    finishSWTest();
});

navigator.serviceWorker.register("resources/postmessage-after-sw-process-crash-worker.js", { }).then(function(registration) {
    worker = registration.installing;
    log("* Sending State to Service Worker");
    worker.postMessage("SetState");
    log("* Asking Service Worker if it received the state");
    worker.postMessage("HasState");
});
