description("Tests that drawImage() of an HDR image into a float16 canvas preserves HDR values, and that an SDR canvas is unaffected.");

window.jsTestIsAsync = true;

const colorSpace = canvas.dataset.colorSpace;
const renderingMode = canvas.dataset.renderingMode;
debug(`canvas.dataset.colorSpace: ${colorSpace}`);
debug(`canvas.dataset.renderingMode: ${renderingMode}`);

const componentsPerPixel = 4;
const canvasSize = 4;
// Most cases draw into a sub-rect, so that drawImage() does not take the rectContainsCanvas()
// path; the cases that do want that path draw over the whole canvas instead.
const drawSize = canvasSize / 2;

const hdrImageURL = "../../images/resources/gainmap-red-green-1920x1920.jpg";
const sdrImageURL = "../../images/resources/green-400x400.png";
const animatedImageURL = "../../images/resources/animated-red-green-blue-400x400.gif";

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
            // Makes Image::hasHDRContent() report true without relying on the image's own
            // metadata: for the gain-map image this covers ports that do not pick the gain
            // map up, and for the animated image there is no HDR metadata to pick up at all.
            if (markAsHDR && window.internals)
                internals.setHasHDRContentForTesting(image);
            resolve(image);
        };
        image.onerror = () => reject(new Error(`Failed to load ${url}`));
        image.src = url;
    });
}

function loadSVGImage(url)
{
    return new Promise((resolve, reject) => {
        const svgNamespace = "http://www.w3.org/2000/svg";
        const svg = document.createElementNS(svgNamespace, "svg");
        // Keep the element out of the way of the test output; it still gets a renderer, and
        // the source rect comes from the image's intrinsic size rather than these attributes.
        svg.setAttribute("style", "position: absolute; visibility: hidden");
        svg.setAttribute("width", canvasSize);
        svg.setAttribute("height", canvasSize);
        const image = document.createElementNS(svgNamespace, "image");
        image.setAttribute("width", canvasSize);
        image.setAttribute("height", canvasSize);
        image.addEventListener("load", () => resolve(image));
        image.addEventListener("error", () => reject(new Error(`Failed to load ${url} into an <svg:image>`)));
        image.setAttribute("href", url);
        svg.appendChild(image);
        document.body.appendChild(svg);
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

// The maximum component value that an SDR (unorm8) buffer can represent, plus a small
// allowance for the color-space conversion that drawing performs.
const sdrMaximum = 1 + 1.5 / 255;

// Depending on the different code paths (GPU process, etc.), the extended-sRGB encoding that
// getImageData() returns spans roughly 1.68 to 2.04; allow some tolerance around that.
const hdrMinimum = 1.4;
const hdrMaximum = 2.5;

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

function drawAndVerify(label, image, destinationColorType, composite, expectHDR, coversCanvas = false)
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
        destination.drawImage(image, -1, -1, canvasSize + 2, canvasSize + 2);
        verifyDrawnArea(label, destination, expectHDR, canvasSize);
    } else {
        destination.drawImage(image, 0, 0, drawSize, drawSize);
        verifyDrawnArea(label, destination, expectHDR, drawSize);
    }
}

const composites = ["source-over", "copy", "source-in"];
const colorTypes = ["float16", "unorm8"];

async function runTests()
{
    let hdrImage;
    let sdrImage;
    let animatedImage;
    let hdrSVGImage;
    try {
        hdrImage = await loadImage(hdrImageURL, true);
        sdrImage = await loadImage(sdrImageURL, false);
        animatedImage = await loadImage(animatedImageURL, true);
        hdrSVGImage = await loadSVGImage(hdrImageURL);
    } catch (error) {
        testFailed(`${error.message}`);
        finishJSTest();
        return;
    }

    for (const composite of composites) {
        debug(`\n- globalCompositeOperation = "${composite}"`);

        for (const destinationColorType of colorTypes)
            drawAndVerify(`HDR image -> ${destinationColorType} ("${composite}")`, hdrImage, destinationColorType, composite, destinationColorType == "float16");

        for (const destinationColorType of colorTypes)
            drawAndVerify(`SDR image -> ${destinationColorType} ("${composite}")`, sdrImage, destinationColorType, composite, false);
    }

    // Test drawImage()'s rectContainsCanvas() branch; that's before the composite operation is considered, so one composite operation is enough to cover it.
    debug(`\n- destination rect covers the canvas`);
    for (const destinationColorType of colorTypes)
        drawAndVerify(`HDR image -> ${destinationColorType} (covers canvas)`, hdrImage, destinationColorType, "source-over", destinationColorType == "float16", true);
    for (const destinationColorType of colorTypes)
        drawAndVerify(`SDR image -> ${destinationColorType} (covers canvas)`, sdrImage, destinationColorType, "source-over", false, true);

    // FIXME: Drawing an animated image draws its first frame, currently only the SDR base.
    // This should trigger when support is added; just update the expected results.
    debug(`\n- animated image reporting HDR content`);
    for (const destinationColorType of colorTypes)
        drawAndVerify(`animated HDR image -> ${destinationColorType}`, animatedImage, destinationColorType, "source-over", false);

    debug(`\n- SVGImageElement source`);
    for (const destinationColorType of colorTypes)
        drawAndVerify(`HDR <svg:image> -> ${destinationColorType}`, hdrSVGImage, destinationColorType, "source-over", destinationColorType == "float16");

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
