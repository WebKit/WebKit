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
        // Triggering potential prefligh through custom header.
        try {
            var response = await fetch("http://localhost:8080/resources/square100.png.fromserviceworker", {headers: {"custom": "header"}});
            log("Failed: fetch suceeded unexpectedly");
        } catch(e) {
            log("PASS: Fetch failed as expected with: " + e);
        }        

        await navigator.serviceWorker.register("resources/service-worker-crossorigin-fetch-worker.js", { });

        var response = await fetch("http://localhost:8080/resources/square100.png.fromserviceworker", {headers: {"custom": "header"}});
        var buffer =  await response.arrayBuffer();
        log("PASS: Got response with buffer byte length being " + buffer.byteLength);

        await logStatus();
    } catch(e) {
        log("Got exception: " + e);
    }
    finishSWTest();
}

test();
