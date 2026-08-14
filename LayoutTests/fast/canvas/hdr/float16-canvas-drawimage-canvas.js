description("Tests that drawImage() from a 2D canvas preserves HDR (extended-range float16) values, and clamps them when the destination is SDR.");

// Exercises CanvasRenderingContext2DBase::drawImage(CanvasBase&, ...) for every
// combination of float16 (HDR) and unorm8 (SDR) source/destination, across the three
// distinct code paths in that function:
//   1. "simple"     - a plain drawImageBuffer(), no intermediate buffer.
//   2. "copy-self"  - globalCompositeOperation="copy" drawing a canvas onto itself,
//                     which copies through an intermediate buffer.
//   3. "source-in"  - a full-canvas composite mode, which composites through an
//                     intermediate buffer in fullCanvasCompositedDrawImage().

const colorSpace = canvas.dataset.colorSpace;
const renderingMode = canvas.dataset.renderingMode;
debug(`canvas.dataset.colorSpace: ${colorSpace}`);
debug(`canvas.dataset.renderingMode: ${renderingMode}`);

const componentsPerPixel = 4;
// A float16 destination stores the drawn values essentially exactly, so half of a unorm8
// color component unit is plenty.
const float16_tolerance = 0.5 / 255;
// A unorm8 destination quantizes to 1/255 steps (up to half a unit of representation
// error), and drawing an extended-range source into it additionally requires CG to convert
// out-of-range values into the destination's non-extended color space. That conversion has
// been observed to shift a component by up to one unorm8 unit, so allow both.
const unorm8_tolerance = 1.5 / 255;

function toleranceFor(destinationColorType)
{
    return destinationColorType == "float16" ? float16_tolerance : unorm8_tolerance;
}

const canvasSize = 4;
// Draw into a sub-rect, so that drawImage() does not take the rectContainsCanvas() path.
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

function fill(context, red)
{
    const imageData = context.createImageData(canvasSize, canvasSize, { pixelFormat: "rgba-float16" });
    for (let pixel = 0; pixel < canvasSize * canvasSize; ++pixel) {
        imageData.data[pixel * componentsPerPixel + 0] = red;
        imageData.data[pixel * componentsPerPixel + 1] = green;
        imageData.data[pixel * componentsPerPixel + 2] = blue;
        imageData.data[pixel * componentsPerPixel + 3] = alpha;
    }
    context.putImageData(imageData, 0, 0);
    return context;
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

function sourceRed(sourceColorType)
{
    return sourceColorType == "float16" ? hdrRed : green;
}

function expectedRed(sourceColorType, destinationColorType)
{
    if (sourceColorType != "float16")
        return green;
    return destinationColorType == "float16" ? hdrRed : 1;
}

function testCrossCanvas(path, composite, sourceColorType, destinationColorType)
{
    const source = createContext(sourceColorType);
    const destination = createContext(destinationColorType);
    if (!source || !destination)
        return;

    fill(source, sourceRed(sourceColorType));
    fill(destination, green);

    destination.globalCompositeOperation = composite;
    destination.drawImage(source.canvas, 0, 0, drawSize, drawSize);
    verifyPixel(`${sourceColorType} -> ${destinationColorType} (${path})`, destination, expectedRed(sourceColorType, destinationColorType), toleranceFor(destinationColorType));
}

function testSelfDraw(path, composite, colorType)
{
    const context = createContext(colorType);
    if (!context)
        return;

    fill(context, sourceRed(colorType));

    context.globalCompositeOperation = composite;
    context.drawImage(context.canvas, 0, 0, drawSize, drawSize);
    verifyPixel(`${colorType} self-draw (${path})`, context, expectedRed(colorType, colorType), toleranceFor(colorType));
}

const colorTypes = ["float16", "unorm8"];

debug('\nSimple drawImage (globalCompositeOperation = "source-over")');
for (const sourceColorType of colorTypes) {
    for (const destinationColorType of colorTypes)
        testCrossCanvas("simple", "source-over", sourceColorType, destinationColorType);
}

debug('\nSelf-copy (globalCompositeOperation = "copy")');
for (const colorType of colorTypes)
    testSelfDraw("copy-self", "copy", colorType);

debug('\nFull-canvas composite (globalCompositeOperation = "source-in")');
for (const sourceColorType of colorTypes) {
    for (const destinationColorType of colorTypes)
        testCrossCanvas("source-in", "source-in", sourceColorType, destinationColorType);
}
for (const colorType of colorTypes)
    testSelfDraw("source-in", "source-in", colorType);

debug('\nCross-canvas copy (globalCompositeOperation = "copy")');
for (const sourceColorType of colorTypes) {
    for (const destinationColorType of colorTypes)
        testCrossCanvas("copy-cross", "copy", sourceColorType, destinationColorType);
}
