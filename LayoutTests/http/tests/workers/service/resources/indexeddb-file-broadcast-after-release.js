log("A File posted to a BroadcastChannel should still be readable after the process that posted it is gone.");

async function doTest()
{
    if (window.testRunner) {
        testRunner.setUseSeparateServiceWorkerProcess(true);
        await fetch("").then(() => { }, () => { });
    }

    const registration = await registerAndWaitForActive("resources/indexeddb-file-blob-retention-worker.js", "resources/broadcast-after-release/");

    const receiver = new Worker("resources/indexeddb-file-broadcast-receiver-worker.js");
    let resolveBlocking;
    const blocking = new Promise((resolve) => resolveBlocking = resolve);
    const readResult = new Promise((resolve) => {
        receiver.onmessage = (event) => event.data === "blocking" ? resolveBlocking() : resolve(event.data);
    });
    receiver.postMessage({ milliseconds: 1500 });
    await blocking;

    registration.active.postMessage({ command: "broadcast-file" });
    await new Promise((resolve) => navigator.serviceWorker.addEventListener("message", resolve, { once: true }));

    testRunner?.terminateServiceWorkers();

    log(await readResult);
    finishSWTest();
}

doTest();
