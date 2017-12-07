var messageNumber = 1;
navigator.serviceWorker.addEventListener("message", function(event) {
    log("PASS: Client received message from service worker, origin: " + event.origin);
    log(event.data);
    if (messageNumber == 1) {
        log("* Simulating Service Worker process crash");
        testRunner.terminateServiceWorkerProcess();
        setTimeout(function() {
            log("* Sending 'Message 2' to Service Worker");
            event.source.postMessage("Message 2");
            messageNumber++;
            handle = setTimeout(function() {
                log("FAIL: Did not receive message from service worker process after the crash");
                finishSWTest();
            }, 1000);
        }, 0);
        return;
    }
    if (messageNumber == 2) {
        clearTimeout(handle);
        finishSWTest();
    }
});

navigator.serviceWorker.register("resources/postmessage-echo-worker.js", { }).then(function(registration) {
    log("* Sending 'Message 1' to Service Worker");
    registration.installing.postMessage("Message 1");
});
