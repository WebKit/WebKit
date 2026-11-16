// Tests that a canvas resized larger than the implementation limits gets a drawing buffer clamped to
// the limits, without errors or a context loss, and that the clamped drawing buffer renders, is read
// and is displayed with the clamped size.
//
// The output does not print the implementation limits, so that the expectations are the same on all
// implementations.
//
// Requires js-test.js and webgl-test-utils.js.

// The errors that the GPU process reports are printed asynchronously, which would make the output
// depend on timing. The tests check the errors with getError(). Workers do not have internals.settings,
// so the page disables the errors before starting the worker.
if (self.internals && internals.settings)
    internals.settings.setWebGLErrorsToConsoleEnabled(false);

var wtu = WebGLTestUtils;
var canvas;
var gl;
var extension;
var attributes;
var readCanvas;
var readContext;
var texture;
var contextLostEventCount;
// The checks evaluate their expressions in the global scope, so the values they use are global.
var limit;
var glLimit;
var expectedWidth;
var expectedHeight;
var bufferWidth;
var bufferHeight;
var viewportBeforeResize;
// Whether the tests use an OffscreenCanvas instead of a canvas element.
var useOffscreenCanvas = false;

// The drawing buffer size limits. The limit of the compositor surfaces is not visible to the page,
// so it is read through internals when available.
function drawingBufferLimits()
{
    var maxTextureSize = gl.getParameter(gl.MAX_TEXTURE_SIZE);
    var maxRenderbufferSize = gl.getParameter(gl.MAX_RENDERBUFFER_SIZE);
    var maxViewportDims = gl.getParameter(gl.MAX_VIEWPORT_DIMS);
    var glLimit = [Math.min(maxTextureSize, maxRenderbufferSize, maxViewportDims[0]), Math.min(maxTextureSize, maxRenderbufferSize, maxViewportDims[1])];
    var limit = glLimit.slice();
    var compositorLimit = null;
    if (self.internals && internals.webglMaxDrawingBufferSize)
        compositorLimit = internals.webglMaxDrawingBufferSize(gl);
    if (compositorLimit)
        limit = [Math.min(limit[0], compositorLimit[0]), Math.min(limit[1], compositorLimit[1])];
    return { glLimit: glLimit, limit: limit };
}

function readPixel(x, y)
{
    var pixel = new Uint8Array(4);
    gl.readPixels(x, y, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
    return pixel.toString();
}

// The animation frame callbacks run before the canvases are presented in the rendering update. A
// task posted from the callback runs after the rendering update.
function afterNextRenderingUpdate()
{
    return new Promise(resolve => requestAnimationFrame(() => setTimeout(resolve, 0)));
}

// Presents the canvas. transferToImageBitmap() uses the display buffer, like presenting a canvas
// element.
async function present()
{
    if (useOffscreenCanvas) {
        // transferToImageBitmap() throws for a canvas with a zero width or height.
        if (canvas.width && canvas.height)
            canvas.transferToImageBitmap().close();
        return;
    }
    await afterNextRenderingUpdate();
}

// After presenting, the drawing buffer is cleared unless it is preserved. transferToImageBitmap()
// clears the drawing buffer regardless of preserveDrawingBuffer.
function isPreservedAfterPresent()
{
    return attributes.preserveDrawingBuffer && !useOffscreenCanvas;
}

function createCanvas(width, height)
{
    if (useOffscreenCanvas)
        return new OffscreenCanvas(width, height);
    var canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    return canvas;
}

function loseAndRestoreContext()
{
    return new Promise(function(resolve, reject) {
        canvas.addEventListener("webglcontextlost", function(event) {
            event.preventDefault();
            setTimeout(function() { extension.restoreContext(); }, 0);
        }, { once: true });
        canvas.addEventListener("webglcontextrestored", function() { resolve(); }, { once: true });
        setTimeout(function() { reject(new Error("timed out waiting for the context to be restored")); }, 10000);
        extension.loseContext();
    });
}

// Resizes the canvas and checks the state right after the resize, before anything is drawn. The
// sizes are compared as booleans, so that the output does not print the limits.
function resize(width, height, newExpectedWidth, newExpectedHeight)
{
    // The GPU process may have lost the context without the page having observed it yet.
    var viewport = gl.getParameter(gl.VIEWPORT);
    if (!viewport) {
        testFailed("the context was lost before the resize");
        return;
    }
    viewportBeforeResize = Array.from(viewport);
    canvas.width = width;
    canvas.height = height;
    expectedWidth = newExpectedWidth;
    expectedHeight = newExpectedHeight;
    shouldBeTrue("gl.drawingBufferWidth == expectedWidth");
    shouldBeTrue("gl.drawingBufferHeight == expectedHeight");
    shouldBeFalse("gl.isContextLost()");
    if (gl.isContextLost())
        return;
    wtu.glErrorShouldBe(gl, gl.NO_ERROR, "after the resize");
    shouldBe("Array.from(gl.getParameter(gl.VIEWPORT))", "viewportBeforeResize");
}

// Renders to the whole drawing buffer and checks that it is read and displayed with its full size.
// The pixels are read with readPixel() instead of wtu.checkCanvasRect(), since the latter clips the
// rectangle to the canvas size, which is not the drawing buffer size for a zero size canvas.
async function checkRendersAtFullSize()
{
    bufferWidth = gl.drawingBufferWidth;
    bufferHeight = gl.drawingBufferHeight;
    gl.viewport(0, 0, bufferWidth, bufferHeight);
    gl.clearColor(0, 1, 0, 1);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT | gl.STENCIL_BUFFER_BIT);
    shouldBeEqualToString("readPixel(0, 0)", "0,255,0,255");
    shouldBeEqualToString("readPixel(bufferWidth - 1, bufferHeight - 1)", "0,255,0,255");
    texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    wtu.shouldGenerateGLError(gl, gl.NO_ERROR, "gl.copyTexImage2D(gl.TEXTURE_2D, 0, attributes.alpha ? gl.RGBA : gl.RGB, 0, 0, bufferWidth, bufferHeight, 0)");
    gl.bindTexture(gl.TEXTURE_2D, null);
    gl.deleteTexture(texture);
    texture = null;

    // A canvas with a zero width or height can't be drawn to another canvas.
    var canDrawCanvas = canvas.width && canvas.height;
    // drawImage() reads the drawing buffer through the compositing result buffer, so the contents
    // must cover the whole clamped size, not the size before the resize.
    if (canDrawCanvas)
        checkCanvasContents("drawImage() of the canvas", [0, 255, 0, 255]);

    await present();
    shouldBeFalse("gl.isContextLost()");

    if (canDrawCanvas)
        checkCanvasContents("drawImage() of the canvas after presenting", isPreservedAfterPresent() ? [0, 255, 0, 255] : (attributes.alpha ? [0, 0, 0, 0] : [0, 0, 0, 255]));
    shouldBe("contextLostEventCount", "0");
}

function checkCanvasContents(label, color)
{
    readCanvas = createCanvas(bufferWidth, bufferHeight);
    readContext = readCanvas.getContext("2d");
    readContext.drawImage(canvas, 0, 0, bufferWidth, bufferHeight);
    wtu.checkCanvasRect(readContext, 0, 0, 1, 1, color, label + " at the origin is " + color);
    wtu.checkCanvasRect(readContext, bufferWidth - 1, bufferHeight - 1, 1, 1, color, label + " at the far corner is " + color);
    readContext = null;
    readCanvas = null;
}

// Loses the context, then drops the canvas, the context and the objects of the previous
// configuration and collects them, so that the number of the live contexts does not grow to the
// limit after which the implementation recycles the oldest contexts.
function releaseContext()
{
    if (extension)
        extension.loseContext();
    if (canvas && canvas.parentNode)
        canvas.parentNode.removeChild(canvas);
    canvas = null;
    gl = null;
    extension = null;
    attributes = null;
    readCanvas = null;
    readContext = null;
    texture = null;
    gc();
}

async function runConfiguration(version, parameters)
{
    releaseContext();
    debug("");
    debug((useOffscreenCanvas ? "OffscreenCanvas, " : "") + "WebGL " + version + ", alpha: " + parameters.alpha + ", antialias: " + parameters.antialias + ", preserveDrawingBuffer: " + parameters.preserveDrawingBuffer + ", depth: " + parameters.depth + ", stencil: " + parameters.stencil);

    canvas = createCanvas(2, 2);
    if (!useOffscreenCanvas) {
        // Keep the layer small regardless of the drawing buffer size.
        canvas.style.width = "2px";
        canvas.style.height = "2px";
        document.body.appendChild(canvas);
    }
    contextLostEventCount = 0;
    var thisCanvas = canvas;
    canvas.addEventListener("webglcontextlost", () => { if (canvas == thisCanvas) ++contextLostEventCount; });
    gl = canvas.getContext(version == 2 ? "webgl2" : "webgl", parameters);
    if (!gl) {
        testFailed("Unable to create the context.");
        return;
    }
    extension = gl.getExtension("WEBGL_lose_context");
    attributes = gl.getContextAttributes();
    if (attributes.antialias != parameters.antialias) {
        debug("Skipped: antialias is not available.");
        return;
    }

    ({ limit, glLimit } = drawingBufferLimits());
    debug("limits:");
    shouldBeTrue("limit[0] <= glLimit[0] && limit[1] <= glLimit[1]");

    // The other dimension is kept small, so that a failure can come only from the limit, not from
    // running out of memory.
    var cases = [
        { label: "small size", size: [4, 4], expected: [4, 4] },
        { label: "width at the limit", size: [limit[0], 2], expected: [limit[0], 2] },
        { label: "width over the limit", size: [limit[0] + 1, 2], expected: [limit[0], 2] },
        { label: "width far over the limit", size: [100000, 2], expected: [limit[0], 2] },
        { label: "height over the limit", size: [2, limit[1] + 1], expected: [2, limit[1]] },
        { label: "zero size", size: [0, 0], expected: [1, 1] },
        { label: "back to a small size", size: [2, 2], expected: [2, 2] },
    ];
    for (var testCase of cases) {
        if (gl.isContextLost()) {
            testFailed(testCase.label + ": the context was lost before the case");
            return;
        }
        debug(testCase.label + ": after the resize:");
        resize(testCase.size[0], testCase.size[1], testCase.expected[0], testCase.expected[1]);
        debug(testCase.label + ": after drawing:");
        await checkRendersAtFullSize();
    }

    if (gl.isContextLost()) {
        testFailed("the context was lost before the repeated resizes");
        return;
    }
    // Resizing over the limit repeatedly without drawing.
    debug("repeated resizes over the limit:");
    for (var i = 0; i < 3; ++i)
        resize(limit[0] + 1 + i, 2, limit[0], 2);

    // A restored context gets the same clamped size.
    await loseAndRestoreContext();
    contextLostEventCount = 0;
    debug("restored with a canvas over the limit:");
    shouldBeTrue("gl.drawingBufferWidth == limit[0]");
    shouldBe("gl.drawingBufferHeight", "2");
    debug("restored with a canvas over the limit: after drawing:");
    await checkRendersAtFullSize();

    // A tall drawing buffer at the height limit followed by a wide one at the width limit. The
    // canvas is resized with separate width and height assignments, so the intermediate size is at
    // both limits. That size must not be allocated, as it may not fit in memory.
    for (var testCase of [
        { label: "tall at the limit", size: [2, limit[1]], expected: [2, limit[1]] },
        { label: "wide at the limit after tall at the limit", size: [limit[0], 2], expected: [limit[0], 2] },
    ]) {
        if (gl.isContextLost()) {
            testFailed(testCase.label + ": the context was lost before the case");
            return;
        }
        debug(testCase.label + ": after the resize:");
        resize(testCase.size[0], testCase.size[1], testCase.expected[0], testCase.expected[1]);
        if (gl.isContextLost())
            return;
        debug(testCase.label + ": after drawing:");
        await checkRendersAtFullSize();
    }
}

// The context configurations for WebGL 1 and WebGL 2.
function generateConfigurations()
{
    var configurations = [];
    for (var version of [1, 2]) {
        for (var antialias of [false, true]) {
            for (var preserveDrawingBuffer of [false, true]) {
                for (var depthStencil of [[false, false], [true, false], [false, true], [true, true]])
                    configurations.push({ version: version, parameters: { alpha: true, antialias: antialias, preserveDrawingBuffer: preserveDrawingBuffer, depth: depthStencil[0], stencil: depthStencil[1] } });
            }
        }
        configurations.push({ version: version, parameters: { alpha: false, antialias: false, preserveDrawingBuffer: false, depth: true, stencil: false } });
        configurations.push({ version: version, parameters: { alpha: false, antialias: true, preserveDrawingBuffer: false, depth: true, stencil: false } });
    }
    return configurations;
}

// Runs slice number `slice` of `sliceCount` equal slices of the runs of every context
// configuration with a canvas element, an OffscreenCanvas or both, as given by the
// useOffscreenCanvas values. A failure of one configuration is reported, and the next one runs.
async function runConfigurations(useOffscreenCanvasValues, slice, sliceCount)
{
    var runs = [];
    for (var useOffscreenCanvasValue of useOffscreenCanvasValues) {
        for (var configuration of generateConfigurations())
            runs.push({ useOffscreenCanvas: useOffscreenCanvasValue, configuration: configuration });
    }
    runs = runs.slice(Math.floor(runs.length * (slice - 1) / sliceCount), Math.floor(runs.length * slice / sliceCount));

    for (var run of runs) {
        useOffscreenCanvas = run.useOffscreenCanvas;
        try {
            await runConfiguration(run.configuration.version, run.configuration.parameters);
        } catch (error) {
            testFailed("" + error);
        }
    }
    releaseContext();
    finishJSTest();
}

// Runs a slice of the tests with a canvas element and, when available, with an OffscreenCanvas.
function runDefaultFramebufferSizeLimitTests(slice, sliceCount)
{
    return runConfigurations(self.OffscreenCanvas ? [false, true] : [false], slice, sliceCount);
}

// Runs a slice of the tests with an OffscreenCanvas, for example in a worker.
function runOffscreenDefaultFramebufferSizeLimitTests(slice, sliceCount)
{
    return runConfigurations([true], slice, sliceCount);
}
