let ctx = null;

let redImage = new Image;
redImage.src = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAAXNSR0IArs4c6QAAABNJREFUCB1j/M/AAEQMDEwgAgQAHxcCAmtAm/sAAAAASUVORK5CYII=";

let blueImage = new Image;
blueImage.src = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAAXNSR0IArs4c6QAAABRJREFUCB1jZGD4/58BCJhABAgAAB0ZAgJSPDJ6AAAAAElFTkSuQmCC";

let transparentImage = new Image;
transparentImage.src = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAAXNSR0IArs4c6QAAAAtJREFUCB1jYEAHAAASAAGAFMrMAAAAAElFTkSuQmCC";

// Blank canvas
let canvas = document.createElement("canvas");
canvas.width = 2;
canvas.height = 2;

function load({offscreen} = {}) {
    if (offscreen) {
        if (window.OffscreenCanvas)
            ctx = new OffscreenCanvas(2, 2).getContext("bitmaprenderer");
    } else
        ctx = canvas.getContext("bitmaprenderer");

    cancelActions();

    runTest();
}

function ignoreException(func){
    try {
        func();
    } catch (e) { }
}

let requestAnimationFrameId = NaN;

function cancelActions() {
    cancelAnimationFrame(requestAnimationFrameId);
    requestAnimationFrameId = NaN;

    createImageBitmap(transparentImage).then((transparentBitmap) => {
        ctx.transferFromImageBitmap(transparentBitmap);
    });
}

async function performActions() {
    let redBitmap = await createImageBitmap(redImage);

    let frames = [
        () => {
            ctx.transferFromImageBitmap(redBitmap);
        },
        () => {
            ctx.canvas.width;
            ctx.canvas.width = 2;
        },
        () => {
            ctx.canvas.height;
            ctx.canvas.height = 2;
        },
        () => {
            TestPage.dispatchEventToFrontend("LastFrame");
        },
    ];
    let index = 0;
    function executeFrameFunction() {
        frames[index++]();
        if (index < frames.length)
            requestAnimationFrameId = requestAnimationFrame(executeFrameFunction);
    };
    executeFrameFunction();
}

async function performConsoleActions() {
    let [redBitmap, blueBitmap] = await Promise.all([
        createImageBitmap(redImage),
        createImageBitmap(blueImage),
    ]);

    console.record(ctx, {name: "TEST"});

    ctx.transferFromImageBitmap(redBitmap);

    console.recordEnd(ctx);

    ctx.transferFromImageBitmap(blueBitmap);
}
