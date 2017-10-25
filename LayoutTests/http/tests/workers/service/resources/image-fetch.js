function done()
{
    finishSWTest();
}

async function loadedImage()
{
    log("PASS: Loaded image");
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
        await navigator.serviceWorker.register("resources/image-fetch-worker.js", { });

        await logStatus();
        image.src = "/resources/square100.png.fromserviceworker";
        await logStatus();
    } catch(e) {
        log("Got exception: " + e);
        await logStatus();
    }
}

test();
