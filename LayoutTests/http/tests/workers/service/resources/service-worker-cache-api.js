function done()
{
    finishSWTest();
}

async function logStatus()
{
    var response = await fetch("status");
    log("Status is " + response.statusText);
}

async function test()
{
    setTimeout(finishSWTest, 10000);
    try {
        log("Registering service worker");
        await navigator.serviceWorker.register("resources/service-worker-cache-api-worker.js", { });
        log("Service worker registered");

        await logStatus();

        log("Fetching");
        var response = await fetch("/resources/square100.png.fromserviceworker");
        var buffer =  await response.arrayBuffer();
        log("Response buffer byte length is " + buffer.byteLength);

        await logStatus();
    } catch(e) {
        await logStatus();
        log("Got exception: " + e);
    }
    finishSWTest();
}

test();
