log("A File posted to a MessagePort that is still in transit to another process should be readable once that process takes it.");

async function doTest()
{
    if (window.testRunner) {
        testRunner.setUseSeparateServiceWorkerProcess(true);
        await fetch("").then(() => { }, () => { });
    }

    const registration = await registerAndWaitForActive("resources/indexeddb-file-blob-retention-worker.js", "resources/message-port-in-transit/");
    const worker = registration.active;

    let file = await fileFromIndexedDB("message-port-in-transit-db");

    const channel = new MessageChannel();
    worker.postMessage({ command: "hold-port" }, [channel.port2]);
    channel.port1.postMessage(file);
    file = null;

    await new Promise((resolve) => navigator.serviceWorker.addEventListener("message", resolve, { once: true }));

    await releaseFile();

    navigator.serviceWorker.addEventListener("message", (event) => {
        log(event.data);
        finishSWTest();
    }, { once: true });
    worker.postMessage({ command: "read-port" });
}

doTest();
