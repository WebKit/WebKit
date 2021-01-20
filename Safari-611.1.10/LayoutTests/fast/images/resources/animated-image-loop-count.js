function drawFrame(image, canvasId) {
    return new Promise((resolve) => {
        let canvas = document.getElementById("canvas-" + canvasId);
        let context = canvas.getContext("2d");
        context.drawImage(image, 0, 0, canvas.width, canvas.height);
        setTimeout(() => {
            resolve(String.fromCharCode(canvasId.charCodeAt() + 1));
        }, 30);
    });
}

function drawImage(image, canvasId, frameCount) {
    let promise = drawFrame(image, canvasId);
    for (let frame = 1; frame < frameCount; ++frame) {
        promise = promise.then((canvasId) => {
            return drawFrame(image, canvasId);
        });
    }
    return promise;
}

function loadImage(src, canvasId, frameCount) {
    return new Promise((resolve) => {
        let image = new Image;
        image.onload = (() => {
            drawImage(image, canvasId, frameCount).then(resolve);
        });
        image.src = src;
    });
}

function runTest(images) {
    if (window.internals) {
        internals.clearMemoryCache();
        internals.settings.setAnimatedImageDebugCanvasDrawingEnabled(true);
    }

    if (window.testRunner)
        testRunner.waitUntilDone();

    var promises = [];

    for (let image of images)
        promises.push(loadImage(image.src, image.canvasId, image.frameCount));
            
    Promise.all(promises).then(() => {
        if (window.testRunner)
            testRunner.notifyDone();
    });
}
