async function test()
{
    try {
        var frame = await interceptedFrame("resources/service-worker-fetch-worker.js", "/");
        var fetch = frame.contentWindow.fetch;

        var response = await fetch("status");
        log("Status is " + response.statusText);

        response = await fetch("/resources/square100.png.fromserviceworker");
        var buffer =  await response.arrayBuffer();
        log("Got image response with buffer byte length being " + buffer.byteLength);

        response = await fetch("/resources/square100.png.bodyasanemptystream");
        var buffer =  await response.arrayBuffer();
        log("Got should-be-empty response with buffer byte length being " + buffer.byteLength + " and status is " + response.statusText);

        response = await fetch("status");
        log("Status is " + response.statusText);
    } catch(e) {
        log("Got exception: " + e);
    }
    finishSWTest();
}

test();
