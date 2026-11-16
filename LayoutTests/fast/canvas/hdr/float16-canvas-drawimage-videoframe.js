description("Tests that drawImage() of an HDR VideoFrame preserves its HDR values in a float16 canvas and clamps them in an SDR one, and that createImageBitmap() of the frame behaves the same way.");

// Covers CanvasRenderingContext2DBase::drawImage(WebCodecsVideoFrame&, ...) and the
// WebCodecsVideoFrame source of ImageBitmap::createCompletionHandler(). Both ask
// usesITUR_2100TF() whether the frame's transfer function is PQ or HLG, and the values only
// survive because VideoFrame::copyNativeImage() converts an HDR frame through a half-float pixel
// format instead of 8-bit BGRA.
//
// drawImage() of a VideoFrame ignores globalCompositeOperation and srcRect today (see the FIXMEs
// in GraphicsContext::drawVideoFrame() and in the canvas drawImage() overload), so unlike the
// image and ImageBitmap tests this one only uses source-over and always draws the whole frame.

window.jsTestIsAsync = true;

const colorSpace = canvas.dataset.colorSpace;
const renderingMode = canvas.dataset.renderingMode;
debug(`canvas.dataset.colorSpace: ${colorSpace}`);
debug(`canvas.dataset.renderingMode: ${renderingMode}`);

const componentsPerPixel = 4;
// The video is 4K and drawImage() of a VideoFrame cannot crop, so the whole frame is scaled into
// the destination. Keep the destination large enough that averaging cannot wash the HDR region
// down below the SDR range.
const canvasSize = 128;
const drawSize = canvasSize / 2;

const hdrVideoURL = "../resources/hdr.mp4";

// The maximum component value an SDR (unorm8) buffer can represent, plus a small allowance for
// the color-space conversion that drawing performs.
const sdrMaximum = 1 + 1.5 / 255;

// This video's HLG content peaks between 1.9971 and 2.3828 in the extended encoding getImageData()
// returns, measured across every configuration this test runs in: both destination color spaces,
// accelerated and unaccelerated, with and without the GPU process, and through both drawImage() and
// createImageBitmap(). These bounds leave room around that span, so they survive changes in how the
// platform applies the transfer function while still catching it being applied at the wrong
// strength, or not at all, which would land near 1.0.
const hdrMinimum = 1.6;
const hdrMaximum = 3.0;

// An in-range color for the SDR control frame.
const sdrFillStyle = "rgb(0, 153, 0)";

function createContext(colorType, size = canvasSize)
{
    const element = document.createElement("canvas");
    element.width = size;
    element.height = size;
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

// Mirrors what fast/canvas/canvas-drawImage-hdr-video.html does: register the frame callback
// before setting src, and do not play the video, just wait for its first presented frame.
function loadVideo(url)
{
    const video = document.createElement("video");
    return new Promise((resolve, reject) => {
        video.requestVideoFrameCallback(() => resolve(video));
        video.onerror = () => reject(new Error(`Failed to load ${url}`));
        video.src = url;
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

// Components can legitimately be negative: BT.2020 content converted into an extended sRGB or
// Display P3 space falls outside that gamut, so only the maximum is meaningful here.
function verifyDrawnArea(label, context, expectHDR)
{
    const { maxComponent, maxAlpha, brightestPixel } = maxComponentInDrawnArea(context, drawSize);

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

function drawAndVerify(label, source, destinationColorType, expectHDR)
{
    const destination = createContext(destinationColorType);
    if (!destination)
        return;

    destination.drawImage(source, 0, 0, drawSize, drawSize);
    verifyDrawnArea(label, destination, expectHDR);
}

// An SDR VideoFrame, so that the SDR expectations are not just "the HDR path did nothing".
function createSDRVideoFrame()
{
    const source = createContext("unorm8", drawSize);
    if (!source)
        return null;
    source.fillStyle = sdrFillStyle;
    source.fillRect(0, 0, drawSize, drawSize);
    return new VideoFrame(source.canvas, { timestamp: 0 });
}

// A VideoFrame whose color space is deliberately incomplete, to cover the fallback in
// VideoFrame::copyNativeImage(): videoFramePickColorSpace() returns an explicit init verbatim, so
// giving only the matrix leaves primaries and transfer unset. The frame's color space then cannot
// describe one on its own and the pixel buffer's own attachments are used instead. Note that
// PlatformVideoColorSpace::isValid() would be true here, which is why copyNativeImage() tests for
// primaries and transfer rather than validity. The contents are plain SDR green, so the draw is
// expected to land in the SDR range whichever way the fallback resolves.
function createIncompleteColorSpaceVideoFrame()
{
    const size = 16;
    const data = new Uint8Array(size * size * componentsPerPixel);
    for (let pixel = 0; pixel < size * size; ++pixel) {
        data[pixel * componentsPerPixel + 0] = 0;
        data[pixel * componentsPerPixel + 1] = 153;
        data[pixel * componentsPerPixel + 2] = 0;
        data[pixel * componentsPerPixel + 3] = 255;
    }
    return new VideoFrame(data, {
        format: "RGBA",
        codedWidth: size,
        codedHeight: size,
        timestamp: 0,
        colorSpace: { matrix: "rgb" },
    });
}

const colorTypes = ["float16", "unorm8"];

async function runTests()
{
    let hdrFrame;
    let sdrFrame;
    try {
        hdrFrame = new VideoFrame(await loadVideo(hdrVideoURL));
        sdrFrame = createSDRVideoFrame();
    } catch (error) {
        testFailed(`${error.message}`);
        finishJSTest();
        return;
    }

    // If the frame is not PQ or HLG then usesITUR_2100TF() is false by definition and the HDR
    // results below would say nothing about this code path, so report that rather than a failure
    // that looks like a drawing bug.
    const transfer = hdrFrame.colorSpace.transfer;
    if (transfer != "pq" && transfer != "hlg") {
        testFailed(`The video decoded to a frame whose transfer function is "${transfer}", not "pq" or "hlg", so this platform cannot exercise the HDR VideoFrame path.`);
        hdrFrame.close();
        if (sdrFrame)
            sdrFrame.close();
        finishJSTest();
        return;
    }
    debug(`hdrFrame.colorSpace.transfer: ${transfer}`);

    debug(`\n- drawImage() of a VideoFrame`);
    for (const destinationColorType of colorTypes)
        drawAndVerify(`HDR VideoFrame -> ${destinationColorType}`, hdrFrame, destinationColorType, destinationColorType == "float16");
    if (sdrFrame) {
        for (const destinationColorType of colorTypes)
            drawAndVerify(`SDR VideoFrame -> ${destinationColorType}`, sdrFrame, destinationColorType, false);
    }

    debug(`\n- drawImage() of an ImageBitmap made from a VideoFrame`);
    const hdrImageBitmap = await createImageBitmap(hdrFrame);
    for (const destinationColorType of colorTypes)
        drawAndVerify(`HDR ImageBitmap -> ${destinationColorType}`, hdrImageBitmap, destinationColorType, destinationColorType == "float16");
    hdrImageBitmap.close();

    if (sdrFrame) {
        const sdrImageBitmap = await createImageBitmap(sdrFrame);
        for (const destinationColorType of colorTypes)
            drawAndVerify(`SDR ImageBitmap -> ${destinationColorType}`, sdrImageBitmap, destinationColorType, false);
        sdrImageBitmap.close();
        sdrFrame.close();
    }

    debug(`\n- VideoFrame whose color space is incomplete`);
    const incompleteFrame = createIncompleteColorSpaceVideoFrame();
    for (const destinationColorType of colorTypes)
        drawAndVerify(`incomplete-color-space VideoFrame -> ${destinationColorType}`, incompleteFrame, destinationColorType, false);
    const incompleteImageBitmap = await createImageBitmap(incompleteFrame);
    for (const destinationColorType of colorTypes)
        drawAndVerify(`incomplete-color-space ImageBitmap -> ${destinationColorType}`, incompleteImageBitmap, destinationColorType, false);
    incompleteImageBitmap.close();
    incompleteFrame.close();

    hdrFrame.close();
    finishJSTest();
}

if (window.internals) {
    internals.clearMemoryCache();
    internals.setScreenContentsFormatsForTesting(["RGBA8", "RGBA16F"]);
}

// Report anything that escapes runTests() as a failure; letting the promise reject silently would
// leave finishJSTest() uncalled and turn the failure into a timeout.
runTests().catch(error => {
    testFailed(`Unexpected exception: ${error}`);
    finishJSTest();
});
