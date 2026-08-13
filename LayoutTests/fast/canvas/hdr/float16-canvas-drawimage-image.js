description("Tests that drawImage() of an HDR image into a float16 canvas preserves HDR values, and that an SDR canvas is unaffected.");

window.jsTestIsAsync = true;

const colorSpace = canvas.dataset.colorSpace;
const renderingMode = canvas.dataset.renderingMode;
debug(`canvas.dataset.colorSpace: ${colorSpace}`);
debug(`canvas.dataset.renderingMode: ${renderingMode}`);

const componentsPerPixel = 4;
const canvasSize = 4;
// Draw into a sub-rect, so that drawImage() does not take the rectContainsCanvas() path.
const drawSize = canvasSize / 2;

const hdrImageURL = "../../images/resources/gainmap-red-green-1920x1920.jpg";
const sdrImageURL = "../../images/resources/green-400x400.png";

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
    return context;
}

function loadImage(url, markAsHDR)
{
    return new Promise((resolve, reject) => {
        const image = new Image;
        image.onload = () => {
            // Marks the image as having HDR content on platforms/ports where the gain map
            // itself is not picked up. This feeds Image::hasHDRContent().
            if (markAsHDR && window.internals)
                internals.setHasHDRContentForTesting(image);
            resolve(image);
        };
        image.onerror = () => reject(new Error(`Failed to load ${url}`));
        image.src = url;
    });
}

function maxComponentInDrawnArea(context)
{
    const data = context.getImageData(0, 0, drawSize, drawSize, { pixelFormat: "rgba-float16" }).data;
    let maxComponent = -Infinity;
    let brightestPixel = null;
    for (let pixel = 0; pixel < drawSize * drawSize; ++pixel) {
        // Ignore alpha, which is not extended-range.
        for (let component = 0; component < componentsPerPixel - 1; ++component) {
            const value = data[pixel * componentsPerPixel + component];
            if (value > maxComponent) {
                maxComponent = value;
                brightestPixel = Array.from(data.slice(pixel * componentsPerPixel, (pixel + 1) * componentsPerPixel));
            }
        }
    }
    return { maxComponent, brightestPixel };
}

function formatPixel(pixel)
{
    return pixel ? `[${pixel.map(v => v.toFixed(4)).join(", ")}]` : "(none)";
}

// The maximum component value that an SDR (unorm8) buffer can represent, plus a small
// allowance for the color-space conversion that drawing performs.
const sdrMaximum = 1 + 1.5 / 255;

function verifyDrawnArea(label, context, expectHDR)
{
    const { maxComponent, brightestPixel } = maxComponentInDrawnArea(context);

    if (!(maxComponent > 0)) {
        testFailed(`${label}: nothing appears to have been drawn (all components are ${maxComponent}).`);
        return;
    }

    if (expectHDR) {
        if (maxComponent > sdrMaximum)
            testPassed(`${label}: has an extended-range component (max ${maxComponent.toFixed(4)}), brightest pixel ${formatPixel(brightestPixel)}`);
        else
            testFailed(`${label}: expected an extended-range component > ${sdrMaximum.toFixed(4)}, but the maximum was ${maxComponent.toFixed(4)}, brightest pixel ${formatPixel(brightestPixel)}`);
        return;
    }
    if (maxComponent <= sdrMaximum)
        testPassed(`${label}: is within the SDR range (max ${maxComponent.toFixed(4)})`);
    else
        testFailed(`${label}: expected all components <= ${sdrMaximum.toFixed(4)}, but the maximum was ${maxComponent.toFixed(4)}, brightest pixel ${formatPixel(brightestPixel)}`);
}

function drawAndVerify(label, image, destinationColorType, composite, expectHDR)
{
    const destination = createContext(destinationColorType);
    if (!destination)
        return;

    // Give the destination pre-existing opaque contents. "source-in" keeps the source only
    // where the destination is already opaque, so without this it would composite against a
    // transparent canvas and produce nothing at all. The fill is deliberately in the SDR
    // range so that it cannot itself satisfy the extended-range assertions.
    destination.fillStyle = `color(${colorSpace} 0 0 0.5)`;
    destination.fillRect(0, 0, canvasSize, canvasSize);

    destination.globalCompositeOperation = composite;
    destination.drawImage(image, 0, 0, drawSize, drawSize);
    verifyDrawnArea(label, destination, expectHDR);
}

const composites = ["source-over", "copy", "source-in"];
const colorTypes = ["float16", "unorm8"];

async function runTests()
{
    let hdrImage;
    let sdrImage;
    try {
        hdrImage = await loadImage(hdrImageURL, true);
        sdrImage = await loadImage(sdrImageURL, false);
    } catch (error) {
        testFailed(`${error.message}`);
        finishJSTest();
        return;
    }

    for (const composite of composites) {
        debug(`\n=== globalCompositeOperation = "${composite}" ===`);

        // An HDR image only stays HDR in a float16 canvas; a unorm8 canvas clamps it.
        for (const destinationColorType of colorTypes)
            drawAndVerify(`HDR image -> ${destinationColorType} ("${composite}")`, hdrImage, destinationColorType, composite, destinationColorType == "float16");

        // An SDR image is in-range regardless of the destination.
        for (const destinationColorType of colorTypes)
            drawAndVerify(`SDR image -> ${destinationColorType} ("${composite}")`, sdrImage, destinationColorType, composite, false);
    }

    finishJSTest();
}

if (window.internals) {
    internals.clearMemoryCache();
    internals.setScreenContentsFormatsForTesting(["RGBA8", "RGBA16F"]);
}

runTests();
