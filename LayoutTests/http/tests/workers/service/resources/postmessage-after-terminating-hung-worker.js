let state = "WaitingForHang";

navigator.serviceWorker.addEventListener("message", async function(event) {
    log(event.data);
    if (state === "WaitingForHang") {
        log("PASS: ServiceWorker received message: " + event.data);
        log("Service Worker should now be hung");
        log("Terminating service worker...")
        await internals.terminateServiceWorker(worker);
        log("Terminated service worker.");
        state = "WaitingForMessageAfterTerminatingHungServiceWorker"
        handle = setInterval(function() {
            worker.postMessage("MessageAfterTerminatingHungServiceWorker");
        }, 10);
        return;
    }
    if (state === "WaitingForMessageAfterTerminatingHungServiceWorker") {
        log("PASS: Service worker responded to message after hanging and being terminated");
        state = "Done";
        clearInterval(handle);
        finishSWTest();
        return;
    }
});

async function doTest()
{
    if (window.testRunner) {
        testRunner.setUseSeparateServiceWorkerProcess(true);
        await fetch("").then(() => { }, () => { });
    }
 
    const registration = await navigator.serviceWorker.register("resources/postmessage-echo-worker-mayhang.js", { });
    worker = registration.installing;
    worker.postMessage("HANG");
}

doTest();

