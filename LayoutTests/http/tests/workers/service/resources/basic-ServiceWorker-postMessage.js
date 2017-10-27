var messageNumber = 1;
navigator.serviceWorker.addEventListener("message", function(event) {
    log("PASS: Client received message from service worker, origin: " + event.origin);
    log("PASS: " + event.data);
    if (messageNumber == 1) {
        event.source.postMessage("Message 2");
        messageNumber++;
    } else
        finishSWTest();
});

navigator.serviceWorker.register("resources/basic-ServiceWorker-postMessage-worker.js", { }).then(function() {
    navigator.serviceWorker.controller.postMessage("Message 1");
});
