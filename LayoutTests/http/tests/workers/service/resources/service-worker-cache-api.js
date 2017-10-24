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
    try {
        await navigator.serviceWorker.register("resources/service-worker-cache-api-worker.js", { });

        await logStatus();

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
