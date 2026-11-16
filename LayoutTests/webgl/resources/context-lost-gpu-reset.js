// Tests that a draw that runs for too long to complete causes the GPU to reset the work, and that
// the context reports this as a real context loss. The context is restored, and a context created
// after the reset works. Every context that shares the GPU state with the reset context is lost, too.
//
// The tests that use this cause a real GPU reset, so they are run singly.
//
// Requires js-test.js and webgl-test-utils.js.

window.jsTestIsAsync = true;
if (window.internals)
    window.internals.settings.setWebGLErrorsToConsoleEnabled(false);

var wtu = WebGLTestUtils;
var canvas;
var gl;
var otherCanvas;
var otherGL;
var newGL;

// The loop count of the long running shaders. It is a uniform, so the shader compiler cannot remove or
// bound the loops. The loops do not terminate in any reasonable time, so the GPU stops the work.
var longRunningLoopCount = 1000000000;

// The loops of the long running shaders, computing float value from float value.
var longRunningLoopsSource = `
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j < count; ++j)
            value = fract(sin(value + float(i) * 0.001 + float(j)) * 43758.5453);
    }`;

function waitForEvent(target, eventName, timeoutSeconds, handler)
{
    return new Promise((resolve, reject) => {
        var timeout = setTimeout(() => reject(new Error("timed out waiting for " + eventName)), timeoutSeconds * 1000);
        target.addEventListener(eventName, (event) => {
            clearTimeout(timeout);
            if (handler)
                handler(event);
            resolve(event);
        }, { once: true });
    });
}

function checkRenders(context, contextName, color)
{
    context.clearColor(color[0] / 255, color[1] / 255, color[2] / 255, color[3] / 255);
    context.clear(context.COLOR_BUFFER_BIT);
    wtu.checkCanvasRect(context, 0, 0, 1, 1, color, contextName + " renders");
    wtu.glErrorShouldBe(context, context.NO_ERROR, contextName + " has no errors");
}

function createContext()
{
    var newCanvas = document.createElement("canvas");
    newCanvas.width = 2;
    newCanvas.height = 2;
    return newCanvas.getContext("webgl2");
}

// Draws with the program of vertexShaderSource and fragmentShaderSource, where one of the shaders runs
// for too long to complete. The shaders have uniforms int count and float seed.
async function runGPUResetTest(vertexShaderSource, fragmentShaderSource, draw)
{
    gl = createContext();
    if (!gl) {
        testFailed("Unable to fetch the WebGL 2 rendering context for canvas.");
        return;
    }
    canvas = gl.canvas;
    otherGL = createContext();
    otherCanvas = otherGL.canvas;
    checkRenders(gl, "context", [0, 255, 0, 255]);
    checkRenders(otherGL, "other context", [0, 0, 255, 255]);

    var program = wtu.setupProgram(gl, [vertexShaderSource, fragmentShaderSource]);
    if (!program) {
        testFailed("Unable to compile the long running program.");
        return;
    }
    gl.uniform1i(gl.getUniformLocation(program, "count"), longRunningLoopCount);
    gl.uniform1f(gl.getUniformLocation(program, "seed"), 0.5);

    var contextLost = waitForEvent(canvas, "webglcontextlost", 60, (event) => event.preventDefault());
    var contextRestored = waitForEvent(canvas, "webglcontextrestored", 60);
    var otherContextLost = waitForEvent(otherCanvas, "webglcontextlost", 60, (event) => event.preventDefault());
    var otherContextRestored = waitForEvent(otherCanvas, "webglcontextrestored", 60);

    debug("Drawing with the long running shader.");
    draw();
    // Wait for the draw to complete, which happens only after the GPU stopped it.
    var pixel = new Uint8Array(4);
    gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
    // The context notices the reset when it next draws.
    gl.clear(gl.COLOR_BUFFER_BIT);

    await contextLost;
    testPassed("The context was lost.");
    shouldBeTrue("gl.isContextLost()");
    shouldBe("gl.getError()", "gl.CONTEXT_LOST_WEBGL");

    // The other context shares the GPU state with the reset context, so it is lost when it next draws.
    otherGL.clear(otherGL.COLOR_BUFFER_BIT);
    await otherContextLost;
    testPassed("The other context was lost.");
    shouldBeTrue("otherGL.isContextLost()");

    await contextRestored;
    testPassed("The context was restored.");
    shouldBeFalse("gl.isContextLost()");
    checkRenders(gl, "restored context", [0, 255, 0, 255]);

    await otherContextRestored;
    testPassed("The other context was restored.");
    shouldBeFalse("otherGL.isContextLost()");
    checkRenders(otherGL, "restored other context", [0, 0, 255, 255]);

    newGL = createContext();
    shouldBeNonNull("newGL");
    shouldBeFalse("newGL.isContextLost()");
    checkRenders(newGL, "context created after the reset", [255, 255, 0, 255]);
}

function runTest(vertexShaderSource, fragmentShaderSource, draw)
{
    runGPUResetTest(vertexShaderSource, fragmentShaderSource, draw).catch((error) => testFailed("" + error)).finally(finishJSTest);
}
