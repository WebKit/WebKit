async function logStatus()
{
    var response = await fetch("status");
    log("Status is " + response.statusText);
}

async function test()
{
    try {
        log("Registering service worker");
        await navigator.serviceWorker.register("resources/service-worker-importScript-worker.js", { });
        log("Registered service worker");

        await logStatus();
        log("PASS");
    } catch(e) {
        console.log("Got exception: " + e);
    }
    finishSWTest();
}

test();
