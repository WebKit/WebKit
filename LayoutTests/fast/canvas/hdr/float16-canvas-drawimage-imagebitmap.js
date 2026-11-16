description("Tests that createImageBitmap() preserves HDR (extended-range float16) values from its synthesized sources, and that drawImage() of the result keeps them in a float16 canvas and clamps them in an SDR one.");

// Exercises ImageBitmap::createImageBuffer()'s choice of color space and pixel format for each
// source that can already carry extended-range values, then draws the resulting ImageBitmap
// through CanvasRenderingContext2DBase::drawImage(ImageBitmap&, ...).
//
// Every source here is synthesized from known values rather than decoded from a gain-map image,
// so the expected results are exact and do not depend on the platform's HDR image support. The
// gain-map image source is covered separately by
// float16-canvas-drawimage-imagebitmap-image.html.

window.jsTestIsAsync = true;

const colorSpace = canvas.dataset.colorSpace;
const renderingMode = canvas.dataset.renderingMode;
debug(`canvas.dataset.colorSpace: ${colorSpace}`);
debug(`canvas.dataset.renderingMode: ${renderingMode}`);

const componentsPerPixel = 4;
const float16_tolerance = 0.5 / 255;
const unorm8_tolerance = 1.5 / 255;

const canvasSize = 4;
// Draw into a sub-rect, so that drawImage() does not take the rectContainsCanvas() path; the
// case that wants that path draws over the whole canvas instead.
const drawSize = canvasSize / 2;

// An HDR red component, well outside [0,1], plus in-range green/blue.
const hdrRed = 3.5;
const green = 0.6;
const blue = 0;
const alpha = 1;

function createContext(colorType)
{
    const element = document.createElement("canvas");
    element.width = canvasSize;
    element.height = canvasSize;
    const settings = { colorSpace, colorType };
    if (renderingMode)
        settings.renderingModeForTesting = renderingMode;
    const context = element.getContext("2d", settings);
    if (!context) {
        testFailed(`Could not create a "${colorType}" 2D context`);
        return null;
    }
    if (context.getContextAttributes().colorType != colorType)
        testFailed(`Expected colorType "${colorType}", got "${context.getContextAttributes().colorType}"`);
    return context;
}

function makeImageData(context, red)
{
    const imageData = context.createImageData(canvasSize, canvasSize, { pixelFormat: "rgba-float16" });
    for (let pixel = 0; pixel < canvasSize * canvasSize; ++pixel) {
        imageData.data[pixel * componentsPerPixel + 0] = red;
        imageData.data[pixel * componentsPerPixel + 1] = green;
        imageData.data[pixel * componentsPerPixel + 2] = blue;
        imageData.data[pixel * componentsPerPixel + 3] = alpha;
    }
    return imageData;
}

function fill(context, red)
{
    context.putImageData(makeImageData(context, red), 0, 0);
    return context;
}

async function makeSourceImageBitmap(sourceKind, sourceColorType, red)
{
    const sourceContext = createContext(sourceColorType);
    if (!sourceContext)
        return null;

    if (sourceKind == "imageData")
        return createImageBitmap(makeImageData(sourceContext, red));

    fill(sourceContext, red);
    const fromCanvas = await createImageBitmap(sourceContext.canvas);
    if (sourceKind == "canvas")
        return fromCanvas;
    return createImageBitmap(fromCanvas);
}

function verifyPixel(label, context, expectedRed, tolerance)
{
    const data = context.getImageData(0, 0, 1, 1, { pixelFormat: "rgba-float16" }).data;
    const expected = [expectedRed, green, blue, alpha];
    const names = ["red", "green", "blue", "alpha"];
    for (let component = 0; component < componentsPerPixel; ++component) {
        const actual = data[component];
        if (Math.abs(actual - expected[component]) <= tolerance)
            testPassed(`${label}: ${names[component]} is ${expected[component]}`);
        else
            testFailed(`${label}: ${names[component]} should be ${expected[component]}. Was ${actual}.`);
    }
}

function toleranceFor(sourceColorType, destinationColorType)
{
    return sourceColorType == "float16" && destinationColorType == "float16" ? float16_tolerance : unorm8_tolerance;
}

function expectedRed(sourceIsHDR, destinationColorType)
{
    if (!sourceIsHDR)
        return green;
    return destinationColorType == "float16" ? hdrRed : 1;
}

async function drawAndVerify(sourceKind, sourceIsHDR, destinationColorType, composite, coversCanvas = false)
{
    const sourceColorType = sourceIsHDR ? "float16" : "unorm8";
    const imageBitmap = await makeSourceImageBitmap(sourceKind, sourceColorType, sourceIsHDR ? hdrRed : green);
    if (!imageBitmap)
        return;

    const destination = createContext(destinationColorType);
    if (!destination)
        return;

    fill(destination, green);

    destination.globalCompositeOperation = composite;
    if (coversCanvas) {
        // rectContainsCanvas() asks whether the destination quad contains the canvas quad, so
        // overshoot the canvas edges rather than matching them exactly.
        destination.drawImage(imageBitmap, -1, -1, canvasSize + 2, canvasSize + 2);
    } else
        destination.drawImage(imageBitmap, 0, 0, drawSize, drawSize);

    const label = `${sourceKind} ${sourceColorType} -> ${destinationColorType} ("${composite}"${coversCanvas ? ", covers canvas" : ""})`;
    verifyPixel(label, destination, expectedRed(sourceIsHDR, destinationColorType), toleranceFor(sourceColorType, destinationColorType));
}

const composites = ["source-over", "copy", "source-in"];
const colorTypes = ["float16", "unorm8"];

async function runTests()
{
    for (const composite of composites) {
        debug(`\n- ImageData source, globalCompositeOperation = "${composite}"`);
        for (const sourceIsHDR of [true, false]) {
            for (const destinationColorType of colorTypes)
                await drawAndVerify("imageData", sourceIsHDR, destinationColorType, composite);
        }
    }

    for (const sourceKind of ["canvas", "imageBitmap"]) {
        debug(`\n- ${sourceKind} source`);
        for (const sourceIsHDR of [true, false]) {
            for (const destinationColorType of colorTypes)
                await drawAndVerify(sourceKind, sourceIsHDR, destinationColorType, "source-over");
        }
    }

    // drawImage()'s rectContainsCanvas() branch is tested before the composite operation is
    // considered, so one composite operation is enough to cover it.
    debug(`\n- destination rect covers the canvas`);
    for (const destinationColorType of colorTypes)
        await drawAndVerify("imageData", true, destinationColorType, "source-over", true);

    finishJSTest();
}

if (window.internals) {
    internals.clearMemoryCache();
    internals.setScreenContentsFormatsForTesting(["RGBA8", "RGBA16F"]);
}

runTests().catch(error => {
    testFailed(`Unexpected exception: ${error}`);
    finishJSTest();
});
