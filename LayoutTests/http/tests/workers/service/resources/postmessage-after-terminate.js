var messageNumber = 1;
navigator.serviceWorker.addEventListener("message", function(event) {
    log("PASS: Client received message from service worker, origin: " + event.origin);
    log(event.data);
    if (messageNumber == 1) {
        window.internals.terminateServiceWorker(event.source);
        event.source.postMessage("Message 2");
        messageNumber++;
    } else
        finishSWTest();
});

navigator.serviceWorker.register("resources/postmessage-echo-worker.js", { }).then(function(registration) {
    registration.installing.postMessage("Message 1");
});
