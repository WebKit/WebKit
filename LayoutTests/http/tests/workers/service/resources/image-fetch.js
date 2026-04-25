async function test()
{
    log("Registering service worker");
    var frame = await interceptedFrame("/workers/service/resources/cors-image-fetch-worker.js", "/workers/service/resources/cors-image-fetch-iframe.html.fromserviceworker");
    log("Service worker registered");
    window.addEventListener("message", (event) => {
        if (event.data === "end") {
            finishSWTest();
            return;
        }
        log(event.data);
    }, false);
    frame.contentWindow.start("/resources/square100.png.fromserviceworker");
}

test();
