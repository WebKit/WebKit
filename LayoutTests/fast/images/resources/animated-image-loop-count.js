function waitForNextFrame(image) {
    return new Promise((resolve) => {
        if (!window.internals) {
            setTimeout(resolve, 30);
            return;
        }
        let frameIndex = internals.imageFrameIndex(image);
        let start = performance.now();
        (function check() {
            if (internals.imageFrameIndex(image) != frameIndex || performance.now() - start > 200) {
                resolve();
                return;
            }
            setTimeout(check, 5);
        })();
    });
}

function drawFrame(image, canvasId) {
    let canvas = document.getElementById(canvasId);
    let context = canvas.getContext("2d");
    context.drawImage(image, 0, 0, canvas.width, canvas.height);
    return waitForNextFrame(image);
}

function drawImage(image, canvasIds, frameCount) {
    let promise = drawFrame(image, canvasIds[0]);
    for (let frame = 1; frame < frameCount; ++frame) {
        promise = promise.then(() => {
            var index = Math.min(frame, canvasIds.length - 1);
            return drawFrame(image, canvasIds[index]);
        });
    }
    return promise;
}

function loadImage(src, canvasIds, frameCount) {
    return new Promise((resolve) => {
        let image = new Image;
        image.onload = (() => {
            drawImage(image, canvasIds, frameCount).then(resolve);
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
        promises.push(loadImage(image.src, image.canvasIds, image.frameCount));
            
    Promise.all(promises).then(() => {
        if (window.testRunner)
            testRunner.notifyDone();
    });
}
