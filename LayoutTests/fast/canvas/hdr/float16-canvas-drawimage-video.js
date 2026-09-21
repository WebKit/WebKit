description("Tests that drawImage() of an HDR video element preserves its HDR values in a float16 canvas and clamps them in an SDR one, and that createImageBitmap() of the video behaves the same way.");

// drawImage() of a video ignores globalCompositeOperation on the CG fast path, which passes no
// ImagePaintingOptions at all, so this only uses source-over.

window.jsTestIsAsync = true;

const colorSpace = canvas.dataset.colorSpace;
const renderingMode = canvas.dataset.renderingMode;
debug(`canvas.dataset.colorSpace: ${colorSpace}`);
debug(`canvas.dataset.renderingMode: ${renderingMode}`);

const componentsPerPixel = 4;
// The HDR video is 4K and drawImage() scales the whole frame into the destination, so keep the
// destination large enough that averaging cannot wash the HDR region below the SDR range.
const canvasSize = 128;
const drawSize = canvasSize / 2;

const hdrVideoURL = "../resources/hdr.mp4";
const sdrVideoURL = "../../../media/content/test.mp4";

// The maximum component value an SDR (unorm8) buffer can represent, plus a small allowance for the
// color-space conversion that drawing performs.
const sdrMaximum = 1 + 1.5 / 255;

// This video's HLG content peaks between 1.9971 and 2.3828 in the extended encoding getImageData()
// returns, measured across every configuration this test runs in: both destination color spaces,
// accelerated and unaccelerated, with and without the GPU process, and through both drawImage() and
// createImageBitmap(). These bounds leave room around that span, so they survive changes in how the
// platform applies the transfer function while still catching it being applied at the wrong
// strength, or not at all, which would land near 1.0.
const hdrMinimum = 1.6;
const hdrMaximum = 3.0;

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

function loadVideo(url)
{
    const video = document.createElement("video");
    return new Promise((resolve, reject) => {
        video.requestVideoFrameCallback(() => resolve(video));
        video.onerror = () => reject(new Error(`Failed to load ${url}`));
        video.src = url;
    });
}

// There is no way to read a video element's transfer function from script, so borrow WebCodecs
// purely as an observation tool: a VideoFrame built from the element exposes it. Used only to make
// a failure legible, not as part of what is being tested.
function transferFunctionOf(video)
{
    const frame = new VideoFrame(video);
    const transfer = frame.colorSpace.transfer;
    frame.close();
    return transfer;
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

const colorTypes = ["float16", "unorm8"];

async function runTests()
{
    let hdrVideo;
    let sdrVideo;
    try {
        hdrVideo = await loadVideo(hdrVideoURL);
        sdrVideo = await loadVideo(sdrVideoURL);
    } catch (error) {
        testFailed(`${error.message}`);
        finishJSTest();
        return;
    }

    const transfer = transferFunctionOf(hdrVideo);
    if (transfer != "pq" && transfer != "hlg") {
        testFailed(`The video decoded with transfer function "${transfer}", not "pq" or "hlg", so this platform cannot exercise the HDR video path.`);
        finishJSTest();
        return;
    }
    debug(`HDR video transfer function: ${transfer}`);

    debug(`\n- drawImage() of a video element`);
    for (const destinationColorType of colorTypes)
        drawAndVerify(`HDR video -> ${destinationColorType}`, hdrVideo, destinationColorType, destinationColorType == "float16");
    for (const destinationColorType of colorTypes)
        drawAndVerify(`SDR video -> ${destinationColorType}`, sdrVideo, destinationColorType, false);

    debug(`\n- drawImage() of an ImageBitmap made from a video element`);
    const hdrImageBitmap = await createImageBitmap(hdrVideo);
    for (const destinationColorType of colorTypes)
        drawAndVerify(`HDR ImageBitmap -> ${destinationColorType}`, hdrImageBitmap, destinationColorType, destinationColorType == "float16");
    hdrImageBitmap.close();

    const sdrImageBitmap = await createImageBitmap(sdrVideo);
    for (const destinationColorType of colorTypes)
        drawAndVerify(`SDR ImageBitmap -> ${destinationColorType}`, sdrImageBitmap, destinationColorType, false);
    sdrImageBitmap.close();

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
