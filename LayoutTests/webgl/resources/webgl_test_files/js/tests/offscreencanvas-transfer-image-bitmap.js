let wtu = WebGLTestUtils;
function draw(gl) {
    let vs = gl.createShader(gl.VERTEX_SHADER);
    gl.shaderSource(vs, "attribute vec2 a_pos; void main() { gl_Position = vec4(a_pos, 0.0, 1.0); }");
    gl.compileShader(vs);
    let fs = gl.createShader(gl.FRAGMENT_SHADER);
    gl.shaderSource(fs, "precision mediump float; void main() { gl_FragColor = vec4(1., 1., 1., 1.); }");
    gl.compileShader(fs);
    let p = gl.createProgram();
    gl.attachShader(p, vs);
    gl.attachShader(p, fs);
    gl.bindAttribLocation(p, 0, "a_pos");
    gl.linkProgram(p);
    gl.useProgram(p);
    gl.viewport(0, 0, 128, 128);
    let verts = [
        1.0,  1.0,
        -1.0,  1.0,
        -1.0, -1.0,
        -1.0, -1.0,
         1.0, -1.0,
         1.0,  1.0
    ];
    let vbo = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(verts), gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);
    gl.useProgram(null);
}

function testContent({contextType, depth, stencil, antialias, preserveDrawingBuffer}) {
    var canvas = new OffscreenCanvas(128, 128);
    var gl = canvas.getContext(contextType, {depth, stencil, antialias, preserveDrawingBuffer});
    if (!gl)
        return `Skipped, could not create ${contextType} context`;

    // Draw the yellow contents and modify the depth and stencil buffers.
    gl.clearColor(1.0, 1.0, 0.0, 1.0);
    gl.clearDepth(0.0);
    gl.clearStencil(255);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT | gl.STENCIL_BUFFER_BIT);
    let bitmap = canvas.transferToImageBitmap();

    // Test that color buffer was cleared.
    var buf = new Uint8Array(4);
    gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, buf);
    let colorClear = buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0;
    gl.clearColor(0.0, 0.0, 0.0, 0.0);
    gl.clear(gl.COLOR_BUFFER_BIT);

    // Test that depth buffer was cleared to 1.0.
    let depthClear = true;
    if (gl.getContextAttributes().depth) {
        gl.enable(gl.DEPTH_TEST);
        gl.depthFunc(gl.GREATER);
        draw(gl);
        gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, buf);
        depthClear = buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0;
        gl.disable(gl.DEPTH_TEST);
        gl.clear(gl.COLOR_BUFFER_BIT);
    }

    // Test that stencil buffer was cleared to 0.
    let stencilClear = true;
    if (gl.getContextAttributes().stencil) {
        gl.enable(gl.STENCIL_TEST);
        gl.stencilOp(gl.KEEP, gl.KEEP, gl.KEEP);
        gl.stencilFunc(gl.NOTEQUAL, 0, 0xffffffff);
        draw(gl);
        gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, buf);
        stencilClear = buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0;
    }
    // Test that the context does not flip preserveDrawingBuffer.
    let preserveDrawingBufferPreserved = preserveDrawingBuffer == gl.getContextAttributes().preserveDrawingBuffer;
    return { bitmap, colorClear, depthClear, stencilClear, preserveDrawingBufferPreserved };
}

function workerScript(subcase) {
    return `
${draw}
${testContent}
let result = testContent({contextType: "${subcase.contextType}", depth: ${subcase.depth}, stencil: ${subcase.stencil}, antialias: ${subcase.antialias}, preserveDrawingBuffer: ${subcase.preserveDrawingBuffer}});
self.postMessage(result, [ result.bitmap ]);
`;
}

function nestedWorkerScript(subcase) {
    return `
${draw}
${testContent}
${workerScript}
try {
    let subcase = { contextType: "${subcase.contextType}", depth: ${subcase.depth}, stencil: ${subcase.stencil}, antialias: ${subcase.antialias}, preserveDrawingBuffer: ${subcase.preserveDrawingBuffer} };
    let worker = new Worker(URL.createObjectURL(new Blob([workerScript(subcase)])));
    worker.onmessage = function(msg) {
        self.postMessage(msg.data, [msg.data.bitmap]);
    };
} catch (e) {
    self.postMessage("Failed, got exception: " + e);
}
`;
}

function testMain(subcase) {
    return testContent(subcase);
}

function testWorker(subcase) {
    return new Promise((resolve) => {
        let worker = new Worker(URL.createObjectURL(new Blob([workerScript(subcase)])));
        worker.onmessage = function(msg) {
            resolve(msg.data);
        };
      });
}

function testNestedWorker(subcase) {
    return new Promise((resolve) => {
        let worker = new Worker(URL.createObjectURL(new Blob([nestedWorkerScript(subcase)])));
        worker.onmessage = function(msg) {
            resolve(msg.data);
        };
      });
}

let tests = [
    testMain,
    testWorker,
    testNestedWorker,
];

let contextType = wtu.getDefault3DContextVersion() == 2 ? "webgl2" : "webgl";
let subcases = [];
for (let test of tests) {
    for (let preserveDrawingBuffer of [true, false]) {
        for (let antialias of [true, false]) {
            for (let depth of [true, false]) {
                for (let stencil of [true, false])
                    subcases.push({test, contextType, preserveDrawingBuffer, antialias, depth, stencil});
            }
        }
    }
}
// To debug one case:
// subcases = [{preserveDrawingBuffer: false, antialias: false, depth: true, stencil: false, contextType, test: testMain}];

let colorClear;
let depthClear;
let stencilClear;
let preserveDrawingBufferPreserved;

async function runTest() {
    for (let subcase of subcases) {
        debug(`test ${subcase.test.name}, contextType: ${subcase.contextType}, depth: ${subcase.depth}, stencil: ${subcase.stencil}, antialias: ${subcase.antialias}, preserveDrawingBuffer:${subcase.preserveDrawingBuffer}`);
        let result = await subcase.test(subcase);
        if (typeof result === "string") {
            if (result.startsWith("Skipped"))
                debug(result);
            else
                testFailed(result);
            continue;
        }
        testTransferToImageBitmap("webgl", result.bitmap);
        colorClear = result.colorClear;
        shouldBeTrue("colorClear");
        depthClear = result.depthClear;
        shouldBeTrue("depthClear");
        stencilClear = result.stencilClear;
        shouldBeTrue("stencilClear");
        preserveDrawingBufferPreserved = result.preserveDrawingBufferPreserved;
        shouldBeTrue("preserveDrawingBufferPreserved");
    }
    finishTest();
}

function testTransferToImageBitmap(webglContextVersion, bitmap) {
    var internalFormat = "RGBA";
    var pixelFormat = "RGBA";
    var pixelType = "UNSIGNED_BYTE";

    var width = 32;
    var height = 32;
    var canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    var gl = WebGLTestUtils.create3DContext(canvas);
    gl.clearColor(0,0,0,1);
    gl.clearDepth(1);
    gl.disable(gl.BLEND);

    TexImageUtils.setupTexturedQuad(gl, internalFormat);

    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    // Enable writes to the RGBA channels
    gl.colorMask(1, 1, 1, 0);
    var texture = gl.createTexture();
    // Bind the texture to texture unit 0
    gl.bindTexture(gl.TEXTURE_2D, texture);
    // Set up texture parameters
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);

    var targets = [gl.TEXTURE_2D];
    // Upload the image into the texture
    for (var tt = 0; tt < targets.length; ++tt) {
        gl.texImage2D(targets[tt], 0, gl[internalFormat], gl[pixelFormat], gl[pixelType], bitmap);
    }
    for (var tt = 0; tt < targets.length; ++tt) {
        // Draw the triangles
        gl.clearColor(0, 0, 0, 1);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        gl.drawArrays(gl.TRIANGLES, 0, 6);

        var buf = new Uint8Array(width * height * 4);
        gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, buf);
        _checkCanvas(buf, width, height, webglContextVersion);
    }
}

function _checkCanvas(buf, width, height, webglContextVersion)
{
    for (var i = 0; i < width * height; i++) {
        if (buf[i * 4] != 255 || buf[i * 4 + 1] != 255 ||
            buf[i * 4 + 2] != 0 || buf[i * 4 + 3] != 255) {
            testFailed("OffscreenCanvas." + webglContextVersion +
                ": This pixel should be [255, 255, 0, 255], but it is: [" + buf[i * 4] + ", " +
                buf[i * 4 + 1] + ", " + buf[i * 4 + 2] + ", " + buf[i * 4 + 3] + "].");
            return;
        }
    }
    testPassed("TransferToImageBitmap test on OffscreenCanvas." + webglContextVersion + " passed");
}
