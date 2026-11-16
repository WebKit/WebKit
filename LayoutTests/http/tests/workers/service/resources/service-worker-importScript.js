async function test()
{
    try {
        var frame = await interceptedFrame("resources/service-worker-importScript-worker.js", "/workers/service/resources/");
        var fetch = frame.contentWindow.fetch;
        var response = await fetch("status");
        log("Status is " + response.statusText);

        log("PASS");
    } catch(e) {
        console.log("Got exception: " + e);
    }
    finishSWTest();
}

test();
