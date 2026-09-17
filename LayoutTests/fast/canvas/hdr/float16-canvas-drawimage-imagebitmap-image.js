description("Tests that createImageBitmap() of an HDR image preserves its HDR values, and that drawImage() of the result keeps them in a float16 canvas and clamps them in an SDR one.");

// Covers the gain-map image source of ImageBitmap::createCompletionHandler(), which asks the
// image for hasHDRContent() and requests an extended-range buffer to decode into.
//
// Unlike float16-canvas-drawimage-imagebitmap.html, the values here come from the platform's
// gain map application, so they are checked against a range rather than exact numbers, and the
// test only works where HDR images decode correctly. See the expectations for
// fast/canvas/hdr/float16-canvas-drawimage-image-*.html, which are gated the same way.

window.jsTestIsAsync = true;

const colorSpace = canvas.dataset.colorSpace;
const renderingMode = canvas.dataset.renderingMode;
debug(`canvas.dataset.colorSpace: ${colorSpace}`);
debug(`canvas.dataset.renderingMode: ${renderingMode}`);

const componentsPerPixel = 4;
const canvasSize = 4;
const drawSize = canvasSize / 2;

const hdrImageURL = "../../images/resources/gainmap-red-green-1920x1920.jpg";
const sdrImageURL = "../../images/resources/green-400x400.png";

// The maximum component value that an SDR (unorm8) buffer can represent, plus a small allowance
// for the color-space conversion that drawing performs.
const sdrMaximum = 1 + 1.5 / 255;

// Depending on the different code paths (GPU process, etc.), the extended-sRGB encoding that
// getImageData() returns spans roughly 1.68 to 2.04; allow some tolerance around that.
const hdrMinimum = 1.4;
const hdrMaximum = 2.5;

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

function loadImage(url, markAsHDR)
{
    return new Promise((resolve, reject) => {
        const image = new Image;
        image.onload = () => {
            if (markAsHDR && window.internals)
                internals.setHasHDRContentForTesting(image);
            resolve(image);
        };
        image.onerror = () => reject(new Error(`Failed to load ${url}`));
        image.src = url;
    });
}

function maxComponentInDrawnArea(context, drawnSize)
{
    const data = context.getImageData(0, 0, drawnSize, drawnSize, { pixelFormat: "rgba-float16" }).data;
    let maxComponent = -Infinity;
    let maxAlpha = -Infinity;
    let brightestPixel = null;
    for (let pixel = 0; pixel < drawnSize * drawnSize; ++pixel) {
        // Ignore alpha, which is not extended-range.
        for (let component = 0; component < componentsPerPixel - 1; ++component) {
            const value = data[pixel * componentsPerPixel + component];
            if (value > maxComponent) {
                maxComponent = value;
                brightestPixel = Array.from(data.slice(pixel * componentsPerPixel, (pixel + 1) * componentsPerPixel));
            }
        }
        maxAlpha = Math.max(maxAlpha, data[pixel * componentsPerPixel + componentsPerPixel - 1]);
    }
    return { maxComponent, maxAlpha, brightestPixel };
}

function formatPixel(pixel)
{
    return pixel ? `[${pixel.map(v => v.toFixed(4)).join(", ")}]` : "(none)";
}

function verifyDrawnArea(label, context, expectHDR, drawnSize)
{
    const { maxComponent, maxAlpha, brightestPixel } = maxComponentInDrawnArea(context, drawnSize);

    if (!(maxComponent > 0)) {
        testFailed(`${label}: no red, green or blue component is above 0 (maximum alpha ${maxAlpha.toFixed(4)}, first pixel ${formatPixel(brightestPixel)}).`);
        return;
    }

    if (expectHDR) {
        if (maxComponent > hdrMinimum && maxComponent < hdrMaximum)
            testPassed(`${label}: peaks within the expected HDR range`);
        else
            testFailed(`${label}: expected a maximum component in (${hdrMinimum}, ${hdrMaximum}), but it was ${maxComponent.toFixed(4)}, brightest pixel ${formatPixel(brightestPixel)}`);
        return;
    }
    if (maxComponent <= sdrMaximum)
        testPassed(`${label}: is within the SDR range`);
    else
        testFailed(`${label}: expected all components <= ${sdrMaximum.toFixed(4)}, but the maximum was ${maxComponent.toFixed(4)}, brightest pixel ${formatPixel(brightestPixel)}`);
}

function drawAndVerify(label, imageBitmap, destinationColorType, composite, expectHDR, coversCanvas = false)
{
    const destination = createContext(destinationColorType);
    if (!destination)
        return;

    destination.fillStyle = `color(${colorSpace} 0 0 0.5)`;
    destination.fillRect(0, 0, canvasSize, canvasSize);

    destination.globalCompositeOperation = composite;
    if (coversCanvas) {
        // rectContainsCanvas() asks whether the destination quad contains the canvas quad, so
        // overshoot the canvas edges rather than matching them exactly.
        destination.drawImage(imageBitmap, -1, -1, canvasSize + 2, canvasSize + 2);
        verifyDrawnArea(label, destination, expectHDR, canvasSize);
    } else {
        destination.drawImage(imageBitmap, 0, 0, drawSize, drawSize);
        verifyDrawnArea(label, destination, expectHDR, drawSize);
    }
}

const composites = ["source-over", "copy", "source-in"];
const colorTypes = ["float16", "unorm8"];

async function runTests()
{
    let hdrImageBitmap;
    let sdrImageBitmap;
    try {
        hdrImageBitmap = await createImageBitmap(await loadImage(hdrImageURL, true));
        sdrImageBitmap = await createImageBitmap(await loadImage(sdrImageURL, false));
    } catch (error) {
        testFailed(`${error.message}`);
        finishJSTest();
        return;
    }

    for (const composite of composites) {
        debug(`\n- globalCompositeOperation = "${composite}"`);

        for (const destinationColorType of colorTypes)
            drawAndVerify(`HDR ImageBitmap -> ${destinationColorType} ("${composite}")`, hdrImageBitmap, destinationColorType, composite, destinationColorType == "float16");

        for (const destinationColorType of colorTypes)
            drawAndVerify(`SDR ImageBitmap -> ${destinationColorType} ("${composite}")`, sdrImageBitmap, destinationColorType, composite, false);
    }

    // drawImage()'s rectContainsCanvas() branch is tested before the composite operation is
    // considered, so one composite operation is enough to cover it.
    debug(`\n- destination rect covers the canvas`);
    for (const destinationColorType of colorTypes)
        drawAndVerify(`HDR ImageBitmap -> ${destinationColorType} (covers canvas)`, hdrImageBitmap, destinationColorType, "source-over", destinationColorType == "float16", true);

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
