async function loadedImage()
{
    log("PASS: Loaded image");
    await logStatus();
    log("Image size: " + image.width + "x" + image.height);

    canvas.getContext("2d").drawImage(image, 0, 0);
    try {
        canvas.toDataURL("image/jpeg");
        log("FAIL: Image is not tainted");
    } catch (e) {
        log("PASS: canvas toDataURL fails with " + e);
    }
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
        await navigator.serviceWorker.register("resources/cors-image-fetch-worker.js", { });
        log("Service worker registered");

        await logStatus();
        log("Loading image");
        image.src = "http://localhost:8000/resources/square100.png.fromserviceworker";
    } catch(e) {
        log("Got exception: " + e);
        await logStatus();
    }
}

test();
