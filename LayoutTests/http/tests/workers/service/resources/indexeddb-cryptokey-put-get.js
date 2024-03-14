async function test() {
    try {
        var registeration = await registerAndWaitForActive("resources/indexeddb-cryptokey-put-get-worker.js", "/workers/service/resources/");
        let worker = registeration.active;
        worker.postMessage("send keys");
    } catch (e) {
        log("Got exception: " + e);
        finishSWTest();
    }
}


navigator.serviceWorker.addEventListener("message", (event) => {
    try {
        if (event.data.result) {
            log("Test Passed");
            finishSWTest();
        } else {
            log("Test Failed: " + event.data.message);
            finishSWTest();
        }
    } catch (err) {
        log("Test Failed. Exception: " + JSON.stringify(err));
        finishSWTest();
    }
});

test();
