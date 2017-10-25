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
        await navigator.serviceWorker.register("resources/service-worker-fetch-worker.js", { });

        await logStatus();

        var response = await fetch("/resources/square100.png.fromserviceworker");
        var buffer =  await response.arrayBuffer();
        log("Got response with buffer byte length being " + buffer.byteLength);

        await logStatus();
    } catch(e) {
        log("Got exception: " + e);
    }
    finishSWTest();
}

test();
