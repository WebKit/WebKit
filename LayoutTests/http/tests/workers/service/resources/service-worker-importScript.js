async function test()
{
    try {
        log("Registering service worker");
        await navigator.serviceWorker.register("resources/service-worker-importScript-worker.js", { });
        log("Registered service worker");

        var frame = await interceptedFrame("resources/service-worker-fetch-worker.js", "/workers/service/resources/");
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
