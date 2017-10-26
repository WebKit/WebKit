function done()
{
    finishSWTest();
}

async function loadedImage()
{
    log("PASS: Loaded image");
    await logStatus();
    log("Image size: " + image.width + "x" + image.height);
    finishSWTest();
}

async function erroredImage()
{
    log("FAIL: image loading failed");
    await logStatus();
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
        log("Registering service worker");
        await navigator.serviceWorker.register("resources/image-fetch-worker.js", { });
        log("Service worker registered");

        await logStatus();
        log("Loading image");
        image.src = "/resources/square100.png.fromserviceworker";
    } catch(e) {
        log("Got exception: " + e);
        await logStatus();
    }
}

test();
