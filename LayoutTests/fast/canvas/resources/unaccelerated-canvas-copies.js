// Copies of a canvas whose context has willReadFrequently: true (an unaccelerated canvas) taken by
// drawImage(), createPattern() and createImageBitmap(), drawn into other canvases and into the source
// canvas itself. Each copy has to show the contents the source canvas had when the copy was taken.

const width = 8;
const height = 8;

const sourceColor = [0, 128, 0, 255];
const changedSourceColor = [255, 0, 0, 255];
const destinationColor = [0, 0, 255, 255];
const probeColor = [255, 255, 0, 255];

function cssColor(color)
{
    return `rgba(${color[0]}, ${color[1]}, ${color[2]}, ${color[3] / 255})`;
}

function createCanvas(canvasType)
{
    if (canvasType == "OffscreenCanvas")
        return new OffscreenCanvas(width, height);
    const canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    return canvas;
}

function createSourceContext(canvasType)
{
    return createCanvas(canvasType).getContext("2d", { willReadFrequently: true });
}

// The last operation on the source before the copy is taken.
const sourceOperations = {
    fillRect(context, color) {
        context.fillStyle = cssColor(color);
        context.fillRect(0, 0, width, height);
    },
    // A path of one line covering the whole canvas.
    strokeLine(context, color) {
        context.strokeStyle = cssColor(color);
        context.lineWidth = 2 * height;
        context.lineCap = "butt";
        context.beginPath();
        context.moveTo(0, height / 2);
        context.lineTo(width, height / 2);
        context.stroke();
    },
    putImageData(context, color) {
        const imageData = context.createImageData(width, height);
        for (let i = 0; i < imageData.data.length; i += 4)
            imageData.data.set(color, i);
        context.putImageData(imageData, 0, 0);
    },
};

// What changes the source after the copy is taken, before the copy is drawn.
const sourceChanges = {
    none: null,
    fillRect: sourceOperations.fillRect,
    putImageData: sourceOperations.putImageData,
};

function drawCopy(destination, copy)
{
    if (copy.pattern) {
        destination.fillStyle = copy.pattern;
        destination.fillRect(0, 0, width, height);
        return;
    }
    destination.drawImage(copy.image, 0, 0);
}

// Each copy type takes a copy of the source and returns what drawCopy() draws. "drawImage(canvas)"
// draws the source itself, so the copy is taken at each draw.
const copyTypes = {
    "drawImage(canvas)": {
        holdsCopy: false,
        async take(destination, source) { return { image: source.canvas }; },
    },
    "createPattern(canvas)": {
        holdsCopy: true,
        async take(destination, source) { return { pattern: destination.createPattern(source.canvas, "repeat") }; },
    },
    "drawImage(createImageBitmap(canvas))": {
        holdsCopy: true,
        async take(destination, source) { return { image: await createImageBitmap(source.canvas) }; },
    },
    "createPattern(createImageBitmap(canvas))": {
        holdsCopy: true,
        async take(destination, source) { return { pattern: destination.createPattern(await createImageBitmap(source.canvas), "repeat") }; },
    },
};

const destinationTypes = {
    "accelerated canvas": source => createCanvas("HTMLCanvasElement").getContext("2d"),
    "unaccelerated canvas": source => createCanvas("HTMLCanvasElement").getContext("2d", { willReadFrequently: true }),
    "the source canvas": source => source,
};

function prepareDestination(destination, source)
{
    if (destination != source)
        sourceOperations.fillRect(destination, destinationColor);
}

function checkPixels(context, expected)
{
    const data = context.getImageData(0, 0, width, height).data;
    for (let i = 0; i < data.length; i += 4) {
        const actual = [...data.slice(i, i + 4)];
        if (actual.some((value, j) => Math.abs(value - expected[j]) > 1))
            return `pixel ${i / 4} is ${actual}, expected ${expected}`;
    }
    return null;
}

function reportResult(name, failure)
{
    if (failure)
        testFailed(`${name}: ${failure}`);
    else
        testPassed(name);
}

// Waits until `lostCanvas`, an accelerated canvas drawn before the GPU process was terminated, reads
// back blank, and drawing into a new accelerated canvas and reading it back works.
async function waitForGPUProcessRelaunch(lostCanvas)
{
    for (let attempt = 0; attempt < 500; ++attempt) {
        await new Promise(resolve => setTimeout(resolve, 10));
        // Blank also while the old GPU process is gone and no new one has been started.
        if (checkPixels(lostCanvas, [0, 0, 0, 0]))
            continue;
        const probe = createCanvas("HTMLCanvasElement").getContext("2d");
        sourceOperations.fillRect(probe, probeColor);
        if (checkPixels(probe, probeColor))
            continue;
        return true;
    }
    return false;
}
