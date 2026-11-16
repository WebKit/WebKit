navigator.serviceWorker.addEventListener("message", async (event) => {
    if (!(event.data instanceof File)) {
        log("FAIL: did not receive a File, received " + event.data);
        finishSWTest();
        return;
    }
    log("PASS: read '" + await event.data.text() + "' from the File posted by the service worker");
    finishSWTest();
});

async function doTest()
{
    if (window.testRunner) {
        testRunner.setUseSeparateServiceWorkerProcess(true);
        await fetch("").then(() => { }, () => { });
    }

    const registration = await registerAndWaitForActive("resources/indexeddb-file-postmessage-worker.js", "resources/");
    registration.active.postMessage("send-file");
}

doTest();
