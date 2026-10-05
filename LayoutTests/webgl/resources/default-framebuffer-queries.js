// Tests the queries that involve the default framebuffer. Each query must have one deterministic
// answer for a given context configuration. The answer must not depend on whether the default
// framebuffer storage is allocated, on whether the default framebuffer is multisampled or
// preserved, on which compositor buffer is current, or on whether the context was restored.
//
// The output does not print the implementation dependent values, such as the sample count or the
// maximum number of draw buffers, so that the expectations are the same on all implementations.
//
// Requires js-test.js and webgl-test-utils.js.

// The errors that the GPU process reports are printed asynchronously, which would make the output
// depend on timing. The tests check the errors with getError().
if (self.internals)
    internals.settings.setWebGLErrorsToConsoleEnabled(false);

var wtu = WebGLTestUtils;
var canvas;
var gl;
var extension;
var drawBuffersExtension;
var attributes;
var framebuffer;
var renderbuffer;
var readCanvas;
var readContext;
var contextLostEventCount;
// The checks evaluate their expressions in the global scope, so the values they use are global.
var queryResult;
var pixel = new Uint8Array(4);
var stateIndependentQueries;
var sameAnswersAsOtherAntialiasAndPreserveDrawingBufferValues;

function valueString(value)
{
    if (value === null || value === undefined)
        return "" + value;
    if (ArrayBuffer.isView(value))
        return "[" + Array.from(value).join(",") + "]";
    return "" + value;
}

function clearErrors()
{
    while (gl.getError() != gl.NO_ERROR) { }
}

function isWebGL2()
{
    return gl instanceof WebGL2RenderingContext;
}

function maxDrawBuffers()
{
    if (isWebGL2())
        return gl.getParameter(gl.MAX_DRAW_BUFFERS);
    if (drawBuffersExtension)
        return gl.getParameter(drawBuffersExtension.MAX_DRAW_BUFFERS_WEBGL);
    return 0;
}

function drawBuffer0()
{
    return isWebGL2() ? gl.DRAW_BUFFER0 : drawBuffersExtension.DRAW_BUFFER0_WEBGL;
}

function drawBuffersAfterTheFirstAreNone()
{
    for (var i = 1; i < maxDrawBuffers(); ++i) {
        if (gl.getParameter(drawBuffer0() + i) !== gl.NONE)
            return false;
    }
    return true;
}

// Checks that the expression generates the error and returns null.
function shouldGenerateErrorAndBeNull(expression, error)
{
    wtu.shouldGenerateGLError(gl, error, "queryResult = " + expression);
    shouldBeNull("queryResult");
}

// The implementation read format of the default framebuffer. Without alpha, the implementation may
// choose either RGB or RGBA.
function shouldBeDefaultFramebufferReadFormat(expression)
{
    if (attributes.alpha)
        shouldBe(expression, "gl.RGBA");
    else
        shouldBeTrue("[gl.RGB, gl.RGBA].includes(" + expression + ")");
}

// The values of the queries that must stay the same in every state of one configuration. The
// drawing buffer size, the viewport and the scissor box are checked separately, since a resize
// changes them.
function collectStateIndependentQueries()
{
    var result = {};
    var names = ["RED_BITS", "GREEN_BITS", "BLUE_BITS", "ALPHA_BITS", "DEPTH_BITS", "STENCIL_BITS", "SAMPLE_BUFFERS", "SAMPLES", "IMPLEMENTATION_COLOR_READ_FORMAT", "IMPLEMENTATION_COLOR_READ_TYPE"];
    for (var name of names)
        result[name] = valueString(gl.getParameter(gl[name]));
    result.checkFramebufferStatus = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
    if (isWebGL2()) {
        result.checkDrawFramebufferStatus = gl.checkFramebufferStatus(gl.DRAW_FRAMEBUFFER);
        result.checkReadFramebufferStatus = gl.checkFramebufferStatus(gl.READ_FRAMEBUFFER);
        result.READ_BUFFER = gl.getParameter(gl.READ_BUFFER);
    }
    var drawBuffers = [];
    for (var i = 0; i < maxDrawBuffers(); ++i)
        drawBuffers.push(gl.getParameter(drawBuffer0() + i));
    result.drawBuffers = drawBuffers.join(",");
    clearErrors();
    return JSON.stringify(result);
}

// Q1, Q2, Q3 and Q4: bit depths, multisampling, implementation read format and completeness.
function checkFramebufferParameters()
{
    shouldBe("gl.getParameter(gl.RED_BITS)", "8");
    shouldBe("gl.getParameter(gl.GREEN_BITS)", "8");
    shouldBe("gl.getParameter(gl.BLUE_BITS)", "8");
    shouldBe("gl.getParameter(gl.ALPHA_BITS)", attributes.alpha ? "8" : "0");
    if (!attributes.depth)
        shouldBe("gl.getParameter(gl.DEPTH_BITS)", "0");
    else if (isWebGL2())
        shouldBe("gl.getParameter(gl.DEPTH_BITS)", "24");
    else
        shouldBeOneOfValues("gl.getParameter(gl.DEPTH_BITS)", [16, 24]);
    shouldBe("gl.getParameter(gl.STENCIL_BITS)", attributes.stencil ? "8" : "0");

    shouldBe("gl.getParameter(gl.SAMPLE_BUFFERS)", attributes.antialias ? "1" : "0");
    if (attributes.antialias) {
        shouldBeGreaterThan("gl.getParameter(gl.SAMPLES)", "0");
        if (isWebGL2())
            shouldBeTrue("Array.from(gl.getInternalformatParameter(gl.RENDERBUFFER, gl.RGBA8, gl.SAMPLES)).includes(gl.getParameter(gl.SAMPLES))");
    } else
        shouldBe("gl.getParameter(gl.SAMPLES)", "0");

    shouldBeDefaultFramebufferReadFormat("gl.getParameter(gl.IMPLEMENTATION_COLOR_READ_FORMAT)");
    shouldBe("gl.getParameter(gl.IMPLEMENTATION_COLOR_READ_TYPE)", "gl.UNSIGNED_BYTE");

    wtu.framebufferStatusShouldBe(gl, gl.FRAMEBUFFER, gl.FRAMEBUFFER_COMPLETE, "FRAMEBUFFER");
    if (isWebGL2()) {
        wtu.framebufferStatusShouldBe(gl, gl.DRAW_FRAMEBUFFER, gl.FRAMEBUFFER_COMPLETE, "DRAW_FRAMEBUFFER");
        wtu.framebufferStatusShouldBe(gl, gl.READ_FRAMEBUFFER, gl.FRAMEBUFFER_COMPLETE, "READ_FRAMEBUFFER");
    }
}

// Q5 and Q6: getFramebufferAttachmentParameter with the default framebuffer bound. The queries
// that are expected to generate errors run only when includeErrorCases is set, so that they are not
// repeated in every state.
function checkAttachmentParameters(includeErrorCases)
{
    if (!isWebGL2()) {
        if (!includeErrorCases)
            return;
        shouldGenerateErrorAndBeNull("gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)", gl.INVALID_OPERATION);
        shouldGenerateErrorAndBeNull("gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)", gl.INVALID_OPERATION);
        return;
    }
    var sizes = {
        FRAMEBUFFER_ATTACHMENT_RED_SIZE: attachment => attachment == "BACK" ? 8 : 0,
        FRAMEBUFFER_ATTACHMENT_GREEN_SIZE: attachment => attachment == "BACK" ? 8 : 0,
        FRAMEBUFFER_ATTACHMENT_BLUE_SIZE: attachment => attachment == "BACK" ? 8 : 0,
        FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE: attachment => attachment == "BACK" && attributes.alpha ? 8 : 0,
        FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE: attachment => attachment == "DEPTH" ? 24 : 0,
        FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE: attachment => attachment == "STENCIL" ? 8 : 0,
    };
    var notApplicable = ["FRAMEBUFFER_ATTACHMENT_OBJECT_NAME", "FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL", "FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE", "FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER"];
    for (var target of ["FRAMEBUFFER", "DRAW_FRAMEBUFFER", "READ_FRAMEBUFFER"]) {
        for (var attachment of ["BACK", "DEPTH", "STENCIL"]) {
            var exists = attachment == "BACK" || (attachment == "DEPTH" && attributes.depth) || (attachment == "STENCIL" && attributes.stencil);
            var expression = pname => "gl.getFramebufferAttachmentParameter(gl." + target + ", gl." + attachment + ", gl." + pname + ")";
            if (!exists) {
                shouldBe(expression("FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE"), "gl.NONE");
                if (!includeErrorCases)
                    continue;
                shouldGenerateErrorAndBeNull(expression("FRAMEBUFFER_ATTACHMENT_RED_SIZE"), gl.INVALID_OPERATION);
                shouldGenerateErrorAndBeNull(expression("FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE"), gl.INVALID_OPERATION);
                continue;
            }
            shouldBe(expression("FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE"), "gl.FRAMEBUFFER_DEFAULT");
            for (var pname in sizes)
                shouldBe(expression(pname), "" + sizes[pname](attachment));
            shouldBe(expression("FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE"), "gl.UNSIGNED_NORMALIZED");
            shouldBe(expression("FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING"), "gl.LINEAR");
            if (!includeErrorCases)
                continue;
            for (var pname of notApplicable)
                shouldGenerateErrorAndBeNull(expression(pname), gl.INVALID_ENUM);
        }
        if (!includeErrorCases)
            continue;
        for (var attachment of ["COLOR_ATTACHMENT0", "DEPTH_ATTACHMENT", "STENCIL_ATTACHMENT", "DEPTH_STENCIL_ATTACHMENT"])
            shouldGenerateErrorAndBeNull("gl.getFramebufferAttachmentParameter(gl." + target + ", gl." + attachment + ", gl.FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)", gl.INVALID_ENUM);
    }
    // The sizes must agree with the getParameter() bit depths.
    if (attributes.depth)
        shouldBe("gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, gl.DEPTH, gl.FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE)", "gl.getParameter(gl.DEPTH_BITS)");
}

// Q7 and Q8: draw buffer and read buffer state, and the bindings.
function checkDrawReadBuffersAndBindings()
{
    if (maxDrawBuffers()) {
        shouldBe("gl.getParameter(drawBuffer0())", "gl.BACK");
        shouldBeTrue("drawBuffersAfterTheFirstAreNone()");
    }
    if (isWebGL2())
        shouldBe("gl.getParameter(gl.READ_BUFFER)", "gl.BACK");
    shouldBeNull("gl.getParameter(gl.FRAMEBUFFER_BINDING)");
    if (isWebGL2()) {
        shouldBeNull("gl.getParameter(gl.DRAW_FRAMEBUFFER_BINDING)");
        shouldBeNull("gl.getParameter(gl.READ_FRAMEBUFFER_BINDING)");
    }
}

// Q9: the drawing buffer size and the context attributes.
function checkSizeAndAttributes(expectedWidth, expectedHeight)
{
    shouldBe("gl.drawingBufferWidth", "" + expectedWidth);
    shouldBe("gl.drawingBufferHeight", "" + expectedHeight);
    shouldBe("JSON.stringify(gl.getContextAttributes())", "JSON.stringify(attributes)");
}

function checkNoErrorsAndNotLost()
{
    wtu.glErrorShouldBe(gl, gl.NO_ERROR, "after the queries");
    shouldBeFalse("gl.isContextLost()");
    shouldBe("contextLostEventCount", "0");
}

// The size of the allocated default framebuffer storage, or null if it can't be observed.
function allocatedSize()
{
    if (!self.internals || !internals.webglDefaultFramebufferAllocatedSize)
        return null;
    return internals.webglDefaultFramebufferAllocatedSize(gl).join("x");
}

// expectedAllocatedSize is the size of the default framebuffer storage that is allocated in this
// state, or "0x0" if none. The queries must not allocate or reallocate the storage.
function checkState(stateName, expectedWidth, expectedHeight, expectedAllocatedSize)
{
    debug(stateName + ":");
    // Print the read format that the implementation chose, so that the expectations record it. The
    // other states must give the same answer as the fresh state.
    if (stateName == "fresh" && !attributes.alpha)
        debug("IMPLEMENTATION_COLOR_READ_FORMAT: " + wtu.glEnumToString(gl, gl.getParameter(gl.IMPLEMENTATION_COLOR_READ_FORMAT)));
    checkFramebufferParameters();
    checkAttachmentParameters(stateName == "fresh");
    checkDrawReadBuffersAndBindings();
    checkSizeAndAttributes(expectedWidth, expectedHeight);
    checkNoErrorsAndNotLost();
    shouldBe("collectStateIndependentQueries()", "stateIndependentQueries");
    if (allocatedSize() !== null)
        shouldBeEqualToString("allocatedSize()", expectedAllocatedSize);
    // Reading allocates the storage.
    wtu.shouldGenerateGLError(gl, gl.NO_ERROR, "gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel)");
}

// Q10: the viewport and the scissor box are set to the drawing buffer size at creation, and are
// not changed by a resize.
function checkViewportAndScissor(width, height)
{
    var expected = "[0, 0, " + width + ", " + height + "]";
    shouldBe("Array.from(gl.getParameter(gl.VIEWPORT))", expected);
    shouldBe("Array.from(gl.getParameter(gl.SCISSOR_BOX))", expected);
}

// Q7: DRAW_BUFFER0 and READ_BUFFER can be set to NONE and back.
function checkSetDrawAndReadBufferNone()
{
    debug("DRAW_BUFFER0 and READ_BUFFER set to NONE and back:");
    if (maxDrawBuffers()) {
        var drawBuffers = isWebGL2() ? "gl.drawBuffers" : "drawBuffersExtension.drawBuffersWEBGL";
        wtu.shouldGenerateGLError(gl, gl.NO_ERROR, drawBuffers + "([gl.NONE])");
        shouldBe("gl.getParameter(drawBuffer0())", "gl.NONE");
        wtu.shouldGenerateGLError(gl, gl.NO_ERROR, drawBuffers + "([gl.BACK])");
        shouldBe("gl.getParameter(drawBuffer0())", "gl.BACK");
    }
    if (isWebGL2()) {
        wtu.shouldGenerateGLError(gl, gl.NO_ERROR, "gl.readBuffer(gl.NONE)");
        shouldBe("gl.getParameter(gl.READ_BUFFER)", "gl.NONE");
        // Q12: the implementation read format and type of a framebuffer with read buffer NONE.
        shouldGenerateErrorAndBeNull("gl.getParameter(gl.IMPLEMENTATION_COLOR_READ_FORMAT)", gl.INVALID_OPERATION);
        shouldGenerateErrorAndBeNull("gl.getParameter(gl.IMPLEMENTATION_COLOR_READ_TYPE)", gl.INVALID_OPERATION);
        wtu.shouldGenerateGLError(gl, gl.NO_ERROR, "gl.readBuffer(gl.BACK)");
        shouldBe("gl.getParameter(gl.READ_BUFFER)", "gl.BACK");
        shouldBeDefaultFramebufferReadFormat("gl.getParameter(gl.IMPLEMENTATION_COLOR_READ_FORMAT)");
    }
    checkNoErrorsAndNotLost();
}

// Q11: with a user framebuffer bound for drawing or for reading, the queries answer for the
// framebuffer bound to the respective binding point.
function checkMixedBindings()
{
    if (!isWebGL2())
        return;
    framebuffer = gl.createFramebuffer();
    renderbuffer = gl.createRenderbuffer();
    gl.bindRenderbuffer(gl.RENDERBUFFER, renderbuffer);
    gl.renderbufferStorage(gl.RENDERBUFFER, gl.RGBA8, 2, 2);
    gl.bindRenderbuffer(gl.RENDERBUFFER, null);
    gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, framebuffer);
    gl.framebufferRenderbuffer(gl.DRAW_FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.RENDERBUFFER, renderbuffer);

    debug("user framebuffer for drawing, default framebuffer for reading:");
    shouldBeDefaultFramebufferReadFormat("gl.getParameter(gl.IMPLEMENTATION_COLOR_READ_FORMAT)");
    shouldBe("gl.getParameter(gl.DEPTH_BITS)", "0");
    shouldBe("gl.getParameter(gl.STENCIL_BITS)", "0");
    shouldBe("gl.getParameter(gl.ALPHA_BITS)", "8");
    shouldBe("gl.getParameter(gl.SAMPLES)", "0");
    wtu.framebufferStatusShouldBe(gl, gl.READ_FRAMEBUFFER, gl.FRAMEBUFFER_COMPLETE, "READ_FRAMEBUFFER");

    debug("default framebuffer for drawing, user framebuffer for reading:");
    gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, null);
    gl.bindFramebuffer(gl.READ_FRAMEBUFFER, framebuffer);
    shouldBe("gl.getParameter(gl.DEPTH_BITS)", attributes.depth ? "24" : "0");
    shouldBe("gl.getParameter(gl.STENCIL_BITS)", attributes.stencil ? "8" : "0");
    shouldBe("gl.getParameter(gl.ALPHA_BITS)", attributes.alpha ? "8" : "0");
    shouldBe("gl.getParameter(gl.SAMPLE_BUFFERS)", attributes.antialias ? "1" : "0");
    shouldBe("gl.getParameter(gl.IMPLEMENTATION_COLOR_READ_FORMAT)", "gl.RGBA");
    wtu.framebufferStatusShouldBe(gl, gl.DRAW_FRAMEBUFFER, gl.FRAMEBUFFER_COMPLETE, "DRAW_FRAMEBUFFER");

    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.deleteFramebuffer(framebuffer);
    gl.deleteRenderbuffer(renderbuffer);
    framebuffer = null;
    renderbuffer = null;
    checkNoErrorsAndNotLost();
}

// Q13: the queries do not lose a pending error.
function checkPendingErrorIsKept()
{
    debug("a pending error is kept across the queries:");
    gl.enable(0x1234);
    for (var name of ["RED_BITS", "ALPHA_BITS", "DEPTH_BITS", "STENCIL_BITS", "SAMPLES", "SAMPLE_BUFFERS", "IMPLEMENTATION_COLOR_READ_FORMAT", "IMPLEMENTATION_COLOR_READ_TYPE"])
        gl.getParameter(gl[name]);
    gl.checkFramebufferStatus(gl.FRAMEBUFFER);
    wtu.glErrorShouldBe(gl, gl.INVALID_ENUM, "the pending error");
    wtu.glErrorShouldBe(gl, gl.NO_ERROR, "after the pending error");
}

var vertexShaderSource = "attribute vec4 position; void main() { gl_Position = position; gl_PointSize = 1.0; }";
var fragmentShaderSource = "precision mediump float; void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }";

function draw()
{
    gl.clearColor(1, 0, 0, 1);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT | gl.STENCIL_BUFFER_BIT);
    var program = gl.createProgram();
    var vertexShader = gl.createShader(gl.VERTEX_SHADER);
    gl.shaderSource(vertexShader, vertexShaderSource);
    gl.compileShader(vertexShader);
    var fragmentShader = gl.createShader(gl.FRAGMENT_SHADER);
    gl.shaderSource(fragmentShader, fragmentShaderSource);
    gl.compileShader(fragmentShader);
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);
    gl.linkProgram(program);
    gl.useProgram(program);
    gl.disableVertexAttribArray(0);
    gl.vertexAttrib4f(0, 0, 0, 0, 1);
    gl.drawArrays(gl.POINTS, 0, 1);
    gl.useProgram(null);
    gl.deleteProgram(program);
    gl.deleteShader(vertexShader);
    gl.deleteShader(fragmentShader);
}

// The animation frame callbacks run before the canvases are presented in the rendering update. A
// task posted from the callback runs after the rendering update.
function afterNextRenderingUpdate()
{
    return new Promise(resolve => requestAnimationFrame(() => setTimeout(resolve, 0)));
}

async function present()
{
    await afterNextRenderingUpdate();
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

// Loses the context, then drops the canvas, the context and the objects of the previous
// configuration and collects them, so that the number of the live contexts does not grow to the
// limit after which the implementation recycles the oldest contexts.
function releaseContext()
{
    if (gl && !gl.isContextLost()) {
        var loseContext = gl.getExtension("WEBGL_lose_context");
        if (loseContext)
            loseContext.loseContext();
    }
    if (canvas && canvas.parentNode)
        canvas.parentNode.removeChild(canvas);
    canvas = null;
    gl = null;
    extension = null;
    drawBuffersExtension = null;
    attributes = null;
    framebuffer = null;
    renderbuffer = null;
    readCanvas = null;
    readContext = null;
    queryResult = null;
    gc();
}

function configurationString(parameters)
{
    return "alpha: " + parameters.alpha + ", premultipliedAlpha: " + parameters.premultipliedAlpha + ", antialias: " + parameters.antialias + ", preserveDrawingBuffer: " + parameters.preserveDrawingBuffer + ", depth: " + parameters.depth + ", stencil: " + parameters.stencil;
}

// Signatures of the queries per configuration, keyed by the configuration without antialias and
// preserveDrawingBuffer, to check that the aliased, multisampled and preserved default framebuffers
// give the same answers.
var configurationSignatures = {};

function checkSameAsOtherConfigurations(version, parameters)
{
    var key = JSON.stringify({ version: version, alpha: parameters.alpha, premultipliedAlpha: parameters.premultipliedAlpha, depth: parameters.depth, stencil: parameters.stencil });
    var signature = JSON.parse(collectStateIndependentQueries());
    delete signature.SAMPLE_BUFFERS;
    delete signature.SAMPLES;
    signature = JSON.stringify(signature);
    if (!(key in configurationSignatures)) {
        configurationSignatures[key] = signature;
        return;
    }
    sameAnswersAsOtherAntialiasAndPreserveDrawingBufferValues = configurationSignatures[key] == signature;
    shouldBeTrue("sameAnswersAsOtherAntialiasAndPreserveDrawingBufferValues");
}

async function runConfiguration(version, parameters)
{
    releaseContext();
    debug("");
    debug("WebGL " + version + ", " + configurationString(parameters));

    canvas = document.createElement("canvas");
    canvas.width = 2;
    canvas.height = 2;
    document.body.appendChild(canvas);
    contextLostEventCount = 0;
    // Count the context loss events of this canvas only, not the ones of the canvases of the
    // earlier configurations.
    var thisCanvas = canvas;
    canvas.addEventListener("webglcontextlost", () => { if (canvas == thisCanvas) ++contextLostEventCount; });
    gl = canvas.getContext(version == 2 ? "webgl2" : "webgl", parameters);
    if (!gl) {
        testFailed("Unable to create the context.");
        return;
    }
    extension = gl.getExtension("WEBGL_lose_context");
    if (version == 1)
        drawBuffersExtension = gl.getExtension("WEBGL_draw_buffers");
    attributes = gl.getContextAttributes();
    if (attributes.antialias != parameters.antialias) {
        debug("Skipped: antialias is not available.");
        return;
    }

    stateIndependentQueries = collectStateIndependentQueries();
    checkState("fresh", 2, 2, "0x0");
    checkViewportAndScissor(2, 2);
    checkSameAsOtherConfigurations(version, parameters);
    checkSetDrawAndReadBufferNone();
    checkMixedBindings();
    checkPendingErrorIsKept();

    draw();
    checkState("drawn", 2, 2, "2x2");

    await present();
    checkState("presented", 2, 2, "2x2");

    canvas.toDataURL();
    readCanvas = document.createElement("canvas");
    readCanvas.width = 2;
    readCanvas.height = 2;
    readContext = readCanvas.getContext("2d");
    readContext.drawImage(canvas, 0, 0);
    checkState("after internal reads", 2, 2, "2x2");

    canvas.width = 3;
    canvas.height = 3;
    // The storage is reallocated to the new size when it is next used.
    checkState("resized", 3, 3, "2x2");
    checkViewportAndScissor(2, 2);

    await loseAndRestoreContext();
    contextLostEventCount = 0;
    // The extensions must be enabled again after a restore.
    if (version == 1)
        drawBuffersExtension = gl.getExtension("WEBGL_draw_buffers");
    checkState("restored", 3, 3, "0x0");

    canvas.width = 0;
    canvas.height = 0;
    // The readPixels() of the previous state allocated the storage.
    checkState("zero size canvas", 1, 1, "3x3");
}

// Q15: the same queries on an OffscreenCanvas context, on the main thread or in a worker.
// transferToImageBitmap() uses the display buffer, like presenting a canvas element.
async function runOffscreenConfiguration(version, parameters)
{
    releaseContext();
    debug("");
    debug("OffscreenCanvas, WebGL " + version + ", " + configurationString(parameters));

    canvas = new OffscreenCanvas(2, 2);
    contextLostEventCount = 0;
    // Count the context loss events of this canvas only, not the ones of the canvases of the
    // earlier configurations.
    var thisCanvas = canvas;
    canvas.addEventListener("webglcontextlost", () => { if (canvas == thisCanvas) ++contextLostEventCount; });
    gl = canvas.getContext(version == 2 ? "webgl2" : "webgl", parameters);
    if (!gl) {
        testFailed("Unable to create the context.");
        return;
    }
    if (version == 1)
        drawBuffersExtension = gl.getExtension("WEBGL_draw_buffers");
    attributes = gl.getContextAttributes();
    if (attributes.antialias != parameters.antialias) {
        debug("Skipped: antialias is not available.");
        return;
    }

    stateIndependentQueries = collectStateIndependentQueries();
    checkState("fresh", 2, 2, "0x0");

    draw();
    checkState("drawn", 2, 2, "2x2");

    canvas.transferToImageBitmap().close();
    checkState("after transferToImageBitmap()", 2, 2, "2x2");

    canvas.width = 3;
    canvas.height = 3;
    checkState("resized", 3, 3, "2x2");
}

// The context configurations: every combination of the alpha variant, depth, stencil, antialias and
// preserveDrawingBuffer for WebGL 1 and WebGL 2. premultipliedAlpha has no effect without alpha.
function generateConfigurations()
{
    var alphaVariants = [
        { alpha: true, premultipliedAlpha: true },
        { alpha: true, premultipliedAlpha: false },
        { alpha: false, premultipliedAlpha: true },
    ];
    var configurations = [];
    for (var version of [1, 2]) {
        for (var alphaVariant of alphaVariants) {
            for (var depth of [true, false]) {
                for (var stencil of [true, false]) {
                    for (var antialias of [false, true]) {
                        for (var preserveDrawingBuffer of [false, true])
                            configurations.push({ version: version, parameters: { alpha: alphaVariant.alpha, premultipliedAlpha: alphaVariant.premultipliedAlpha, antialias: antialias, preserveDrawingBuffer: preserveDrawingBuffer, depth: depth, stencil: stencil } });
                    }
                }
            }
        }
    }
    return configurations;
}

// Runs slice number `slice` of `sliceCount` equal slices of the runs of each of
// runConfigurationFunctions for every context configuration.
async function runConfigurations(runConfigurationFunctions, slice, sliceCount)
{
    var runs = [];
    for (var runConfigurationFunction of runConfigurationFunctions) {
        for (var configuration of generateConfigurations())
            runs.push({ runConfigurationFunction: runConfigurationFunction, configuration: configuration });
    }
    runs = runs.slice(Math.floor(runs.length * (slice - 1) / sliceCount), Math.floor(runs.length * slice / sliceCount));

    try {
        for (var run of runs)
            await run.runConfigurationFunction(run.configuration.version, run.configuration.parameters);
    } catch (error) {
        testFailed("" + error);
    }
    releaseContext();
    finishJSTest();
}

// Runs a slice of the tests with a canvas element and, when available, with an OffscreenCanvas.
function runDefaultFramebufferQueryTests(slice, sliceCount)
{
    return runConfigurations(self.OffscreenCanvas ? [runConfiguration, runOffscreenConfiguration] : [runConfiguration], slice, sliceCount);
}

// Runs a slice of the tests with an OffscreenCanvas, for example in a worker.
function runOffscreenDefaultFramebufferQueryTests(slice, sliceCount)
{
    return runConfigurations([runOffscreenConfiguration], slice, sliceCount);
}
