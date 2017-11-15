async function test()
{
    log("Registering service worker");
    var frame = await interceptedFrame("/workers/service/resources/cors-image-fetch-worker.js", "/workers/service/resources/cors-image-fetch-iframe.html.fromserviceworker");
    log("Service worker registered");
    window.addEventListener("message", (event) => {
        if (event.data === "end") {
            canvas.getContext("2d").drawImage(frame.contentWindow.image, 0, 0);
            try {
                canvas.toDataURL("image/jpeg");
                log("FAIL: Image is not tainted");
            } catch (e) {
                log("PASS: canvas toDataURL fails with " + e);
            }
            finishSWTest();
            return;
        }
        log(event.data);
    }, false);
    frame.contentWindow.start("http://localhost:8000/resources/square100.png.fromserviceworker");
}

test();
