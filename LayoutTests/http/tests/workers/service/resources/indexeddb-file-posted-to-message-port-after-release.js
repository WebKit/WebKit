log("A File posted to a MessagePort should still be readable after the sending process releases its blob URL.");

async function doTest()
{
    if (window.testRunner) {
        testRunner.setUseSeparateServiceWorkerProcess(true);
        await fetch("").then(() => { }, () => { });
    }

    const registration = await registerAndWaitForActive("resources/indexeddb-file-blob-retention-worker.js", "resources/message-port-after-release/");
    const worker = registration.active;

    const channel = new MessageChannel();
    worker.postMessage({ command: "hold-port" }, [channel.port2]);
    await new Promise((resolve) => navigator.serviceWorker.addEventListener("message", resolve, { once: true }));

    let file = await fileFromIndexedDB("message-port-after-release-db");
    channel.port1.postMessage(file);
    file = null;

    await releaseFile();

    navigator.serviceWorker.addEventListener("message", (event) => {
        log(event.data);
        finishSWTest();
    }, { once: true });
    worker.postMessage({ command: "read-port" });
}

doTest();
