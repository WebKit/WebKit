async function test()
{
    try {
        var frame = await interceptedFrame("resources/service-worker-fetch-worker.js", "/");
        var fetch = frame.contentWindow.fetch;

        var response = await fetch("status");
        log("Status is " + response.statusText);

        response = await fetch("/resources/square100.png.fromserviceworker");
        var buffer =  await response.arrayBuffer();
        log("Got response with buffer byte length being " + buffer.byteLength);

        response = await fetch("status");
        log("Status is " + response.statusText);
    } catch(e) {
        log("Got exception: " + e);
    }
    finishSWTest();
}

test();
