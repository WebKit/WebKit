async function test()
{
    try {
        var frame = await interceptedFrame("resources/service-worker-fetch-worker.js", "/workers/service/resources/test.bodyasstream");
        var fetch = frame.contentWindow.fetch;

        var expectedFrameContent = "This test passes if the sentence is complete with PASS.";
        log("Checking frame content received as chunks: " + (frame.contentWindow.document.body.innerHTML === expectedFrameContent ? "PASS" : "FAIL"));

        var response = await fetch("status");
        log("Status is " + response.statusText);

        response = await fetch("/resources/square100.png.fromserviceworker");
        var buffer =  await response.arrayBuffer();
        log("Got image response with buffer byte length being " + buffer.byteLength);

        response = await fetch("/resources/square100.png.bodyasanemptystream");
        buffer =  await response.arrayBuffer();
        log("Got should-be-empty response with buffer byte length being " + buffer.byteLength + " and status is " + response.statusText);

        response = await fetch("data:text/plain;base64,cmVzcG9uc2UncyBib2R5");
        var text  =  await response.text();
        log("Got data URL response with data=\"" + text + "\" and status is " + response.statusText);

        response = await fetch(window.URL.createObjectURL(new Blob(["PASS"])));
        text =  await response.text();
        log("Got blob response with data=\"" + text + "\" and status is " + response.statusText);

        response = await fetch("status");
        log("Status is " + response.statusText);
    } catch(e) {
        log("Got exception: " + e);
    }
    finishSWTest();
}

test();
