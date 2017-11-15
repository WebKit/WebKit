async function test()
{
    try {
        try {
            var response = await fetch("http://localhost:8080/resources/square100.png.fromserviceworker");
            log("Failed: fetch suceeded unexpectedly");
        } catch(e) {
            log("PASS: Fetch failed as expected with: " + e);
        }

        var frame = await interceptedFrame("resources/service-worker-crossorigin-fetch-worker.js", "/");

        var response = await frame.contentWindow.fetch("http://localhost:8080/resources/square100.png.fromserviceworker");
        var buffer =  await response.arrayBuffer();
        log("PASS: Got response with buffer byte length being " + buffer.byteLength);

        if (!frame.contentWindow.internals)
            return Promise.rejects("Test requires internals API");

        await new Promise(resolve => {
            frame.onload = resolve;
            frame.contentWindow.internals.forceReload(true);
        });

        // On shift reload, frame should bypass its service worker.
        try {
            var response = await fetch("http://localhost:8080/resources/square100.png.fromserviceworker");
            log("Failed: fetch suceeded unexpectedly");
        } catch(e) {
            log("PASS: Fetch failed as expected with: " + e);
        }
    } catch(e) {
        log("FAIL: Got exception: " + e);
    }
    finishSWTest();
}
test();
