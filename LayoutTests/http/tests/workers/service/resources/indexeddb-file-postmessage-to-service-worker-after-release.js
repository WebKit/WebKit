log("A File posted to a service worker should still be readable after the sending process releases its blob URL.");

async function doTest()
{
    if (window.testRunner) {
        testRunner.setUseSeparateServiceWorkerProcess(true);
        await fetch("").then(() => { }, () => { });
    }

    const registration = await registerAndWaitForActive("resources/indexeddb-file-blob-retention-worker.js", "resources/postmessage-after-release/");
    const worker = registration.active;

    await blockServiceWorker(worker);

    let file = await fileFromIndexedDB("postmessage-after-release-db");
    worker.postMessage(file);
    file = null;

    navigator.serviceWorker.addEventListener("message", (event) => {
        log(event.data);
        finishSWTest();
    }, { once: true });

    await releaseFile();
}

doTest();
