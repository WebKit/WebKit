var messageNumber = 1;
navigator.serviceWorker.addEventListener("message", async function(event) {
    log("PASS: Client received message from service worker, origin: " + event.origin);
    log(event.data);
    if (messageNumber == 1) {
        await window.internals.terminateServiceWorker(event.source);
        event.source.postMessage("Message 2");
        messageNumber++;
    } else
        finishSWTest();
});

async function doTest()
{
    if (window.testRunner) {
        testRunner.setUseSeparateServiceWorkerProcess(true);
        await fetch("").then(() => { }, () => { });
    }
 
    const registration = await navigator.serviceWorker.register("resources/postmessage-echo-worker.js", { });
    registration.installing.postMessage("Message 1");
}

doTest();
