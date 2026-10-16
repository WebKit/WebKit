// Shared harness for the WebGL microbenchmarks.
//
// The benchmarks use only WebGL 1 features that ANGLE's WebGPU backend supports, so the same
// tests run on the Metal and WebGPU backends. Each benchmark renders into its own framebuffer,
// not the canvas, and checks its result before, during and after measurement:
//
//  1. setup(), then two runs of iteration() followed by probe() and a full verify().
//  2. One warm-up and 10 measured iterations. Each iteration does runs, iteration() followed by
//     probe(), until 100 milliseconds have passed, and at least one, and reports runs per second.
//     The probe reads back one pixel. That is the point where the GPU has to finish the batch, and
//     the pixel is checked against the expected result. A benchmark whose iteration() takes well
//     under a millisecond sets WebGLBench.repeatCount, and each run calls iteration() that many
//     times before the probe, so that the probe's round trip is a small part of the run.
//  3. A full verify() of the last iteration.
//
// A benchmark that needs an extension that is not available is skipped. It reports 0 runs/s for
// every iteration, with a "SKIP:" line and a description that starts with the reason.
//
// Every iteration n writes content derived from n, so cached or dropped work fails the checks.
// An incorrect result does not stop the benchmark, so that the performance of an implementation
// that renders incorrectly can still be measured. The failed checks are printed as "FAIL:" lines
// after the results, which run-perf-tests reports as an error. Other exceptions, such as a shader
// that fails to compile, stop the benchmark.

(function () {
"use strict";

const WebGLBench = {};

// Thrown by the checks of the results. The benchmark records it and continues.
class CheckFailure extends Error { }

// contextType is "webgl" unless a benchmark needs a WebGL 2 function. attributes override the
// default context attributes, which have no antialiasing, depth or stencil buffer.
WebGLBench.createContext = function (contextType, width, height, attributes) {
    const canvas = document.createElement("canvas");
    canvas.width = width || 16;
    canvas.height = height || 16;
    const gl = canvas.getContext(contextType || "webgl", { antialias: false, depth: false, stencil: false, preserveDrawingBuffer: false, ...attributes });
    if (!gl)
        throw new Error("WebGL is not available");
    return gl;
};

// An RGBA8 texture attached to a framebuffer. The framebuffer is left bound with a matching
// viewport.
WebGLBench.createRenderTarget = function (gl, width, height) {
    const texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
    const framebuffer = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, texture, 0);
    const status = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
    if (status != gl.FRAMEBUFFER_COMPLETE)
        throw new Error("Framebuffer is not complete: 0x" + status.toString(16));
    gl.viewport(0, 0, width, height);
    // Leaving the texture bound for sampling would form a feedback loop.
    gl.bindTexture(gl.TEXTURE_2D, null);
    return { texture, framebuffer, width, height };
};

function compileShader(gl, type, source)
{
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS) && !gl.isContextLost())
        throw new Error("Shader compilation failed: " + gl.getShaderInfoLog(shader));
    return shader;
}

// Attribute i of attributeNames is bound to location i. The program is left in use.
WebGLBench.createProgram = function (gl, vertexSource, fragmentSource, attributeNames) {
    const program = gl.createProgram();
    const vertexShader = compileShader(gl, gl.VERTEX_SHADER, vertexSource);
    const fragmentShader = compileShader(gl, gl.FRAGMENT_SHADER, fragmentSource);
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);
    (attributeNames || []).forEach((name, index) => gl.bindAttribLocation(program, index, name));
    gl.linkProgram(program);
    gl.deleteShader(vertexShader);
    gl.deleteShader(fragmentShader);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS) && !gl.isContextLost())
        throw new Error("Program link failed: " + gl.getProgramInfoLog(program));
    gl.useProgram(program);
    return program;
};

// A value unique to this page load, from 1 up to 2 in steps of 1/65536.
const loadSalt = 1 + Math.floor(performance.timeOrigin * 1000) % 65536 / 65536;

// Makes a shader source unique to this page load, without changing what it computes. Benchmarks of
// shader compilation use it so that no shader cache, including Metal's cache on disk, has seen
// their sources in an earlier run. It adds a uniform that is always zero, times a constant unique
// to the page load, to gl_FragColor or gl_Position at the end of main(), which must be the last
// function of the source.
WebGLBench.uniqueToPageLoad = function (source, isFragmentShader) {
    const end = source.lastIndexOf("}");
    const output = isFragmentShader ? "gl_FragColor" : "gl_Position";
    return "uniform mediump float uBenchZero;\n" + source.substring(0, end)
        + "  " + output + " += vec4(uBenchZero * " + loadSalt.toFixed(8) + ");\n" + source.substring(end);
};

// Two triangles covering clip space, as vec2 positions bound to attribute location 0.
WebGLBench.bindFullscreenQuad = function (gl) {
    const buffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]), gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
    return buffer;
};

// Pseudo-random bytes, different for each seed.
WebGLBench.createPattern = function (byteLength, seed) {
    const words = new Uint32Array(Math.ceil(byteLength / 4));
    let state = WebGLBench.hash(seed + 1) || 1;
    for (let i = 0; i < words.length; ++i) {
        state ^= state << 13;
        state ^= state >>> 17;
        state ^= state << 5;
        words[i] = state;
    }
    return new Uint8Array(words.buffer, 0, byteLength);
};

WebGLBench.readPixels = function (gl, target, x, y, width, height, pixels) {
    pixels = pixels || new Uint8Array(width * height * 4);
    gl.bindFramebuffer(gl.FRAMEBUFFER, target.framebuffer);
    gl.readPixels(x, y, width, height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
    return pixels;
};

WebGLBench.hash = function (value) {
    value = Math.imul(value ^ (value >>> 16), 0x45d9f3b);
    value = Math.imul(value ^ (value >>> 16), 0x45d9f3b);
    return (value ^ (value >>> 16)) >>> 0;
};

// An opaque RGBA color, as bytes, for draw i of iteration n.
WebGLBench.colorInto = function (n, i, out) {
    const value = WebGLBench.hash(n * 65599 + i);
    out[0] = value & 0xff;
    out[1] = (value >>> 8) & 0xff;
    out[2] = (value >>> 16) & 0xff;
    out[3] = 255;
    return out;
};

WebGLBench.color = function (n, i) {
    return WebGLBench.colorInto(n, i, [0, 0, 0, 0]);
};

function pixelToString(pixels, offset)
{
    return "(" + pixels[offset] + ", " + pixels[offset + 1] + ", " + pixels[offset + 2] + ", " + pixels[offset + 3] + ")";
}

function pixelMatches(pixels, offset, expected, tolerance)
{
    for (let c = 0; c < 4; ++c) {
        if (Math.abs(pixels[offset + c] - expected[c]) > tolerance)
            return false;
    }
    return true;
}

// Reads back the whole target and compares each pixel with expectedPixel(x, y, out).
WebGLBench.verifyTarget = function (gl, target, label, tolerance, expectedPixel) {
    const pixels = WebGLBench.readPixels(gl, target, 0, 0, target.width, target.height);
    const expected = [0, 0, 0, 0];
    let mismatches = 0;
    let firstMismatch = "";
    for (let y = 0; y < target.height; ++y) {
        for (let x = 0; x < target.width; ++x) {
            const offset = (y * target.width + x) * 4;
            expectedPixel(x, y, expected);
            if (pixelMatches(pixels, offset, expected, tolerance))
                continue;
            if (!mismatches++)
                firstMismatch = " First at (" + x + ", " + y + "): got " + pixelToString(pixels, offset) + ", expected " + pixelToString(expected, 0) + ".";
        }
    }
    if (mismatches)
        throw new CheckFailure(label + ": " + mismatches + " of " + (target.width * target.height) + " pixels are wrong." + firstMismatch);
};

// Compares RGBA pixels, width pixels per row, with the expected bytes.
WebGLBench.verifyPixels = function (label, actual, expected, width, tolerance) {
    let mismatches = 0;
    let firstMismatch = "";
    for (let offset = 0; offset < expected.length; offset += 4) {
        if (Math.abs(actual[offset] - expected[offset]) <= tolerance
            && Math.abs(actual[offset + 1] - expected[offset + 1]) <= tolerance
            && Math.abs(actual[offset + 2] - expected[offset + 2]) <= tolerance
            && Math.abs(actual[offset + 3] - expected[offset + 3]) <= tolerance)
            continue;
        if (!mismatches++) {
            const pixel = offset / 4;
            firstMismatch = " First at (" + (pixel % width) + ", " + Math.floor(pixel / width) + "): got " + pixelToString(actual, offset) + ", expected " + pixelToString(expected, offset) + ".";
        }
    }
    if (mismatches)
        throw new CheckFailure(label + ": " + mismatches + " of " + (expected.length / 4) + " pixels are wrong." + firstMismatch);
};

// Checks that the context got the requested attributes, for example that it has an antialiased
// drawing buffer when antialias is true.
WebGLBench.verifyContextAttributes = function (gl, attributes) {
    const actual = gl.getContextAttributes();
    for (const name in attributes) {
        if (actual[name] !== attributes[name])
            throw new CheckFailure("Context attribute " + name + ": got " + actual[name] + ", expected " + attributes[name] + ".");
    }
};

// Compares RGBA pixels, width pixels per row, with one expected pixel. Faster than verifyPixels()
// for large readbacks, since there is no array of expected bytes to fill.
WebGLBench.verifyPixelsAreColor = function (label, actual, expected, width, tolerance) {
    const [r, g, b, a] = expected;
    let mismatches = 0;
    let firstMismatch = 0;
    for (let offset = 0; offset < actual.length; offset += 4) {
        if (Math.abs(actual[offset] - r) <= tolerance
            && Math.abs(actual[offset + 1] - g) <= tolerance
            && Math.abs(actual[offset + 2] - b) <= tolerance
            && Math.abs(actual[offset + 3] - a) <= tolerance)
            continue;
        if (!mismatches++)
            firstMismatch = offset;
    }
    if (mismatches) {
        const pixel = firstMismatch / 4;
        throw new CheckFailure(label + ": " + mismatches + " of " + (actual.length / 4) + " pixels are wrong. First at (" + (pixel % width) + ", " + Math.floor(pixel / width) + "): got " + pixelToString(actual, firstMismatch) + ", expected " + pixelToString(expected, 0) + ".");
    }
};

// Compares the RGBA pixels of a width x height rectangle at (x, y), read back into actual, which can
// be any typed array, with expected values from a table. expected holds 4 values per entry, and
// expectedIndex(x, y) returns the entry of the pixel at (x, y). tolerances holds the allowed
// difference of each channel, or a function of the expected value that returns it. Comparing with
// a small table, rather than an array of expected pixels, keeps large readbacks fast in Debug
// builds.
WebGLBench.verifyPixelsFromTable = function (label, actual, x, y, width, height, expected, expectedIndex, tolerances) {
    const expectedPixel = [0, 0, 0, 0];
    let mismatches = 0;
    let firstMismatch = "";
    for (let row = 0; row < height; ++row) {
        for (let column = 0; column < width; ++column) {
            const offset = (row * width + column) * 4;
            const entry = expectedIndex(x + column, y + row) * 4;
            let matches = true;
            for (let c = 0; c < 4; ++c) {
                const value = expected[entry + c];
                const allowed = typeof tolerances[c] == "function" ? tolerances[c](value) : tolerances[c];
                if (!(Math.abs(actual[offset + c] - value) <= allowed)) {
                    matches = false;
                    break;
                }
            }
            if (matches || mismatches++)
                continue;
            for (let c = 0; c < 4; ++c)
                expectedPixel[c] = expected[entry + c];
            firstMismatch = " First at (" + (x + column) + ", " + (y + row) + "): got " + pixelToString(actual, offset) + ", expected " + pixelToString(expectedPixel, 0) + ".";
        }
    }
    if (mismatches)
        throw new CheckFailure(label + ": " + mismatches + " of " + (width * height) + " pixels are wrong." + firstMismatch);
};

// Checks that every pixel of the target is exactly color.
WebGLBench.verifyTargetColor = function (gl, target, label, color) {
    WebGLBench.verifyTarget(gl, target, label, 0, (x, y, out) => {
        out[0] = color[0];
        out[1] = color[1];
        out[2] = color[2];
        out[3] = color[3];
    });
};

const probePixel = new Uint8Array(4);

// Reads back one pixel of the target, which waits for the GPU, and compares it with expected.
WebGLBench.probe = function (gl, target, x, y, expected, tolerance) {
    WebGLBench.readPixels(gl, target, x, y, 1, 1, probePixel);
    if (!pixelMatches(probePixel, 0, expected, tolerance || 0))
        throw new CheckFailure("Probe at (" + x + ", " + y + "): got " + pixelToString(probePixel, 0) + ", expected " + pixelToString(expected, 0) + ".");
};

function expectNoError(gl, label)
{
    if (gl.isContextLost())
        throw new CheckFailure(label + ": the context was lost");
    const error = gl.getError();
    if (error != gl.NO_ERROR)
        throw new CheckFailure(label + ": GL error 0x" + error.toString(16));
}

// PerfTestRunner.log() output is buffered until the runner finishes, so failures are written to
// the document directly.
function logLine(line)
{
    let log = document.getElementById("log");
    if (!log) {
        log = document.createElement("pre");
        log.id = "log";
        document.body.appendChild(log);
    }
    log.textContent += line + "\n";
}

function logFailure(message)
{
    logLine("FAIL: " + message);
}

// Records the failed checks of one phase of the benchmark: their count and the first message.
class FailedChecks {
    constructor(phase)
    {
        this.phase = phase;
        this.count = 0;
        this.firstMessage = null;
    }

    check(callback)
    {
        try {
            callback();
        } catch (error) {
            if (!(error instanceof CheckFailure))
                throw error;
            if (!this.count++)
                this.firstMessage = error.message;
        }
    }

    log(total)
    {
        if (this.count)
            logFailure("Incorrect result " + this.phase + " (" + this.count + " of " + total + " checks): " + this.firstMessage);
    }
}

// How many times each run calls iteration() before probe(). iteration() must produce the same
// result every time it is called with the same n.
WebGLBench.repeatCount = 1;

// Each measured iteration does as many runs as fit in this many milliseconds, and at least one.
const iterationTime = 100;
// PerfTestRunner ignores the first iteration as a warm-up, then measures this many.
const measuredIterationCount = 10;

// The rows of a table of WebGLFormats, in webgl-formats.js, as objects with a property per column,
// and with the property group set to the name of the group of rows.
function formatRows(columns, groups)
{
    const rows = [];
    for (const group in groups) {
        for (const values of groups[group]) {
            const row = { group };
            columns.forEach((column, index) => row[column] = values[index]);
            rows.push(row);
        }
    }
    return rows;
}

// The properties of a texture format, the first row of WebGLFormats.textureFormats with
// internalFormat, and with type if it is given.
WebGLBench.textureFormat = function (internalFormat, type) {
    const row = formatRows(WebGLFormats.textureFormatColumns, WebGLFormats.textureFormats)
        .find(row => row.internalFormat == internalFormat && (type === undefined || row.type == type));
    if (!row)
        throw new Error("No texture format " + internalFormat + (type ? " with type " + type : "") + " in WebGLFormats");
    return row;
};

// The properties of a compressed texture format, with extension set to the extension it is in.
WebGLBench.compressedTextureFormat = function (internalFormat) {
    const row = formatRows(WebGLFormats.compressedTextureFormatColumns, WebGLFormats.compressedTextureFormats)
        .find(row => row.internalFormat == internalFormat);
    if (!row)
        throw new Error("No compressed texture format " + internalFormat + " in WebGLFormats");
    row.extension = row.group;
    return row;
};

// What the clear and draw benchmarks need to know about a texture or renderbuffer format that they
// render to: { type, channels } for a color format, { depth, stencil } for a depth or stencil
// format, and extensions, the extensions that the format needs as an attachment. type says how the
// color format is cleared, drawn to and read back: "normalized", "snorm8", "snorm16", "float",
// "int" or "uint". channels is the number of channels it stores.
WebGLBench.attachmentFormat = function (internalFormat) {
    const renderbufferOnly = formatRows(WebGLFormats.renderbufferOnlyFormatColumns, { renderbuffer: WebGLFormats.renderbufferOnlyFormats })
        .find(row => row.internalFormat == internalFormat);
    const row = renderbufferOnly || WebGLBench.textureFormat(internalFormat);
    const extensions = [];
    if (row.extension)
        extensions.push(row.extension);
    if (typeof row.renderable == "string")
        extensions.push(row.renderable);
    const depth = row.channels.some(channel => channel.startsWith("depth"));
    const stencil = row.channels.includes("stencil8");
    if (depth || stencil)
        return { depth, stencil, extensions };
    const first = row.channels[0];
    let type = "float";
    if (first.startsWith("unorm") || first.startsWith("srgb"))
        type = "normalized";
    else if (first.startsWith("snorm"))
        type = first;
    else if (first.startsWith("uint"))
        type = "uint";
    else if (first.startsWith("int"))
        type = "int";
    return { type, channels: row.channels.length, extensions };
};

// Returns the value of the enum called name, a property of the context or of one of the extension
// objects in extensions, which maps extension names to the objects that getExtension() returned.
WebGLBench.enumValue = function (gl, extensions, name) {
    if (gl[name] !== undefined)
        return gl[name];
    for (const extensionName in extensions) {
        if (extensions[extensionName][name] !== undefined)
            return extensions[extensionName][name];
    }
    throw new Error("No enum " + name + " in the context or in the extensions " + Object.keys(extensions).join(", "));
};

// Reports a benchmark that can't run, because an extension it needs is not available, as 0 runs/s
// for every iteration. The reason is in a "SKIP:" line and at the start of the description.
function skip(test, reason)
{
    logLine("SKIP: " + reason);
    PerfTestRunner.reportValues({
        description: "Skipped because " + reason + " " + test.description,
        unit: "runs/s",
        customIterationCount: measuredIterationCount
    }, new Array(measuredIterationCount + 1).fill(0));
}

// test: { description, contextType, canvasSize, contextAttributes, extensions, setup(gl, extensions),
// iteration(gl, n), probe(gl, n), verify(gl, n) }. canvasSize and contextAttributes are optional
// and passed to createContext(), for a benchmark that draws to the default framebuffer. extensions
// lists the names of the extensions that the benchmark needs. run() enables them with
// getExtension() before setup(), and passes setup() an object that maps their names to the
// extension objects. When one is not available, the benchmark is skipped.
WebGLBench.run = function (test) {
    let gl;
    let n = 0;
    const before = new FailedChecks("before measurement");
    const during = new FailedChecks("during measurement");
    const after = new FailedChecks("after measurement");
    let probeCount = 0;
    try {
        gl = WebGLBench.createContext(test.contextType, test.canvasSize, test.canvasSize, test.contextAttributes);
        const extensions = {};
        const missing = [];
        for (const name of test.extensions || []) {
            const extension = gl.getExtension(name);
            if (extension)
                extensions[name] = extension;
            else
                missing.push(name);
        }
        if (missing.length) {
            skip(test, missing.join(" and ") + (missing.length > 1 ? " are" : " is") + " not available.");
            return;
        }
        test.setup(gl, extensions);
        expectNoError(gl, "Setup");
        for (; n < 2; ++n) {
            for (let i = 0; i < WebGLBench.repeatCount; ++i)
                test.iteration(gl, n);
            before.check(() => test.probe(gl, n));
            before.check(() => test.verify(gl, n));
            before.check(() => expectNoError(gl, "Iteration " + n));
        }
    } catch (error) {
        logFailure(error.message);
        if (window.testRunner)
            testRunner.notifyDone();
        return;
    }

    function run()
    {
        ++n;
        for (let i = 0; i < WebGLBench.repeatCount; ++i)
            test.iteration(gl, n);
        ++probeCount;
        during.check(() => test.probe(gl, n));
    }

    function measureIteration()
    {
        PerfTestRunner.gc();
        let runs = 0;
        let elapsed = 0;
        const start = PerfTestRunner.now();
        try {
            do {
                run();
                ++runs;
                elapsed = PerfTestRunner.now() - start;
            } while (elapsed < iterationTime);
        } catch (error) {
            logFailure(error.message);
            if (window.testRunner)
                testRunner.notifyDone();
            return;
        }
        if (PerfTestRunner.measureValueAsync(runs * 1000 / elapsed))
            setTimeout(measureIteration, 0);
    }

    let description = test.description;
    if (WebGLBench.repeatCount > 1)
        description += " Each run does this " + WebGLBench.repeatCount + " times before the readPixels.";
    PerfTestRunner.prepareToMeasureValuesAsync({
        description,
        unit: "runs/s",
        customIterationCount: measuredIterationCount,
        done: function () {
            try {
                after.check(() => test.verify(gl, n));
                after.check(() => expectNoError(gl, "Final iteration " + n));
            } catch (error) {
                logFailure(error.message);
            }
            before.log(6);
            during.log(probeCount);
            after.log(2);
        }
    });
    setTimeout(measureIteration, 0);
};

window.WebGLBench = WebGLBench;
})();
