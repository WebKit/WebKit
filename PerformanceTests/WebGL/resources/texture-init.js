// Each run makes the level of an RGBA8 texture uninitialized, then does one operation that needs
// it initialized, and reads back two pixels. WebGL requires robust resource initialization, so
// before the operation the implementation must zero-fill the level. That cost is what this
// benchmark measures. The operation writes at most a small corner of the level, so filling the
// level is most of the work.
//
// The storage says how the level becomes uninitialized:
//
//  - "new": each run creates a new texture with immutable storage, texStorage2D() or
//    texStorage3D() with one level, and attaches it to a framebuffer that exists for the whole
//    benchmark. It deletes the texture of the previous run first, so that probe() can read the
//    texture of the current run.
//  - "invalidate": setup() creates one texture and attaches it to the framebuffer. Each run binds
//    the framebuffer and invalidates the attachment with invalidateFramebuffer(). Under robust
//    resource initialization, an implementation may treat the invalidated contents as
//    uninitialized. A page that invalidates the attachments of a framebuffer after each frame does
//    this.
//
// The target says what the framebuffer is attached to:
//
//  - "2d": a 2D texture.
//  - "2d-array": layer 1 of a 2D array texture with 4 layers. Robust resource initialization keeps
//    one initialization state for all the layers of a level, so initializing one layer initializes
//    all four.
//
// The operation, after the level became uninitialized:
//
//  - "draw": draws the corner to the framebuffer, with the scissor test.
//  - "sample": draws to a separate 8x8 framebuffer, sampling one texel of the texture. The texel
//    changes from one run to the next.
//  - "sub-upload": replaces the corner with texSubImage2D() or texSubImage3D().
//  - "copy-sub": copies the corner from a separate framebuffer with copyTexSubImage2D() or
//    copyTexSubImage3D().
//  - "blit-sub": copies the corner from a separate framebuffer with blitFramebuffer().
//  - "read": nothing. probe() is the operation: its readPixels() reads the uninitialized level.
//
// The separate framebuffers have 64x64 or 8x8 RGBA8 textures, which each run overwrites completely,
// so they stay initialized and are not part of what is measured. The copies read a source that the
// run clears first.
//
// Check: the corner is the color of iteration n after the writing operations, and the rest of the
// level, the other layers and the corner after the other operations are zero. The 8x8 framebuffer
// of "sample" is the sampled texel, zero, plus the color of iteration n. probe() reads one pixel of
// the corner and one outside of it, or one pixel of the 8x8 framebuffer and one of the texture.
// verify() reads every pixel of every layer.
(function () {
"use strict";

const size = 1024;
const layerCount = 4;
const layer = 1;
// The corner that the operations write.
const cornerSize = 64;
const sampleTargetSize = 8;
const tolerance = 1;
const zero = [0, 0, 0, 0];

const vertexShader = `#version 300 es
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}`;

const drawFragmentShader = `#version 300 es
precision highp float;
uniform vec4 uColor;
out vec4 outColor;
void main() {
    outColor = uColor;
}`;

function sampleFragmentShader(target)
{
    const array = target == "2d-array";
    return `#version 300 es
precision highp float;
uniform highp ${array ? "sampler2DArray" : "sampler2D"} uTexture;
uniform highp ivec2 uTexel;
uniform vec4 uColor;
out vec4 outColor;
void main() {
    outColor = texelFetch(uTexture, ${array ? "ivec3(uTexel, " + layer + ")" : "uTexel"}, 0) + uColor;
}`;
}

const operationDescriptions = {
    draw: "draws a " + cornerSize + "x" + cornerSize + " corner of it with the scissor test",
    sample: "samples one texel of it in a draw to an 8x8 framebuffer",
    "sub-upload": "replaces a " + cornerSize + "x" + cornerSize + " corner of it with texSubImage{dimensions}()",
    "copy-sub": "copies a " + cornerSize + "x" + cornerSize + " corner into it with copyTexSubImage{dimensions}()",
    "blit-sub": "copies a " + cornerSize + "x" + cornerSize + " corner into it with blitFramebuffer()",
    read: "reads back 2 pixels of it",
};

const writingOperations = new Set(["draw", "sub-upload", "copy-sub", "blit-sub"]);

// A pixel of the corner and a pixel outside of it, for run n.
function probePositions(n)
{
    const value = WebGLBench.hash(n);
    const inside = [value % cornerSize, (value >>> 8) % cornerSize];
    const outside = [cornerSize + (value >>> 16) % (size - cornerSize), (value >>> 4) % size];
    return [inside, outside];
}

// options: { storage, target, operation }, see above.
window.runTextureInit = function (options) {
    const array = options.target == "2d-array";
    const textureTarget = array ? "TEXTURE_2D_ARRAY" : "TEXTURE_2D";
    const writes = writingOperations.has(options.operation);
    const dimensions = array ? "3D" : "2D";

    let texture = null;
    let target;
    let checkFramebuffer;
    let source;
    let sampleTarget;
    let program;
    let colorLocation;
    let texelLocation;
    let uploadData;
    let uploadWords;
    const color = [0, 0, 0, 0];

    const textureText = size + "x" + size + " RGBA8 " + (array ? "2D array texture with " + layerCount + " layers" : "texture");
    const storageText = options.storage == "new"
        ? "creates a new " + textureText + " with texStorage" + dimensions + "() and attaches " + (array ? "its layer " + layer : "it") + " to a framebuffer"
        : "invalidates a framebuffer attachment that is " + (array ? "layer " + layer + " of a " : "a ") + textureText;
    const description = "Measures a run that " + storageText + ", then " + operationDescriptions[options.operation].replace("{dimensions}", dimensions)
        + (options.operation == "read" ? "." : ", and reads back 2 pixels.")
        + " The level is uninitialized before the operation, so the implementation zero-fills it" + (array ? ", all " + layerCount + " layers," : "") + " first.";

    function createTexture(gl)
    {
        const newTexture = gl.createTexture();
        gl.bindTexture(gl[textureTarget], newTexture);
        if (array)
            gl.texStorage3D(gl.TEXTURE_2D_ARRAY, 1, gl.RGBA8, size, size, layerCount);
        else
            gl.texStorage2D(gl.TEXTURE_2D, 1, gl.RGBA8, size, size);
        return newTexture;
    }

    function attach(gl, framebuffer, attachedTexture, attachedLayer)
    {
        gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
        if (array)
            gl.framebufferTextureLayer(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, attachedTexture, 0, attachedLayer);
        else
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, attachedTexture, 0);
    }

    function clearSource(gl)
    {
        gl.bindFramebuffer(gl.FRAMEBUFFER, source.framebuffer);
        gl.clearColor(color[0] / 255, color[1] / 255, color[2] / 255, color[3] / 255);
        gl.clear(gl.COLOR_BUFFER_BIT);
    }

    WebGLBench.run({
        description,
        contextType: "webgl2",
        setup(gl) {
            target = { framebuffer: gl.createFramebuffer(), width: size, height: size };
            checkFramebuffer = gl.createFramebuffer();
            if (options.storage == "invalidate") {
                texture = createTexture(gl);
                attach(gl, target.framebuffer, texture, layer);
                const status = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
                if (status != gl.FRAMEBUFFER_COMPLETE)
                    throw new Error("Framebuffer is not complete: 0x" + status.toString(16));
            }

            switch (options.operation) {
            case "draw":
                program = WebGLBench.createProgram(gl, vertexShader, drawFragmentShader);
                colorLocation = gl.getUniformLocation(program, "uColor");
                break;
            case "sample":
                sampleTarget = WebGLBench.createRenderTarget(gl, sampleTargetSize, sampleTargetSize);
                program = WebGLBench.createProgram(gl, vertexShader, sampleFragmentShader(options.target));
                colorLocation = gl.getUniformLocation(program, "uColor");
                texelLocation = gl.getUniformLocation(program, "uTexel");
                gl.uniform1i(gl.getUniformLocation(program, "uTexture"), 0);
                break;
            case "sub-upload":
                uploadData = new Uint8Array(cornerSize * cornerSize * 4);
                uploadWords = new Uint32Array(uploadData.buffer);
                break;
            case "copy-sub":
            case "blit-sub":
                source = WebGLBench.createRenderTarget(gl, cornerSize, cornerSize);
                break;
            }
            // The vertex shader makes the corners from gl_VertexID, and needs no attributes.
            gl.bindVertexArray(gl.createVertexArray());
        },
        iteration(gl, n) {
            if (options.storage == "new") {
                gl.bindFramebuffer(gl.FRAMEBUFFER, target.framebuffer);
                if (texture)
                    gl.deleteTexture(texture);
                texture = createTexture(gl);
                attach(gl, target.framebuffer, texture, layer);
            } else {
                gl.bindFramebuffer(gl.FRAMEBUFFER, target.framebuffer);
                gl.invalidateFramebuffer(gl.FRAMEBUFFER, [gl.COLOR_ATTACHMENT0]);
            }

            WebGLBench.colorInto(n, 0, color);
            switch (options.operation) {
            case "draw":
                gl.useProgram(program);
                gl.viewport(0, 0, size, size);
                gl.enable(gl.SCISSOR_TEST);
                gl.scissor(0, 0, cornerSize, cornerSize);
                gl.uniform4f(colorLocation, color[0] / 255, color[1] / 255, color[2] / 255, color[3] / 255);
                gl.drawArrays(gl.TRIANGLES, 0, 3);
                gl.disable(gl.SCISSOR_TEST);
                break;
            case "sample": {
                const value = WebGLBench.hash(n + 1);
                gl.bindFramebuffer(gl.FRAMEBUFFER, sampleTarget.framebuffer);
                gl.viewport(0, 0, sampleTargetSize, sampleTargetSize);
                gl.useProgram(program);
                gl.activeTexture(gl.TEXTURE0);
                gl.bindTexture(gl[textureTarget], texture);
                gl.uniform2i(texelLocation, value % size, (value >>> 16) % size);
                gl.uniform4f(colorLocation, color[0] / 255, color[1] / 255, color[2] / 255, color[3] / 255);
                gl.drawArrays(gl.TRIANGLES, 0, 3);
                break;
            }
            case "sub-upload":
                uploadWords.fill(color[0] | color[1] << 8 | color[2] << 16 | color[3] << 24);
                gl.bindTexture(gl[textureTarget], texture);
                if (array)
                    gl.texSubImage3D(gl.TEXTURE_2D_ARRAY, 0, 0, 0, layer, cornerSize, cornerSize, 1, gl.RGBA, gl.UNSIGNED_BYTE, uploadData);
                else
                    gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, cornerSize, cornerSize, gl.RGBA, gl.UNSIGNED_BYTE, uploadData);
                break;
            case "copy-sub":
                clearSource(gl);
                gl.bindTexture(gl[textureTarget], texture);
                if (array)
                    gl.copyTexSubImage3D(gl.TEXTURE_2D_ARRAY, 0, 0, 0, layer, 0, 0, cornerSize, cornerSize);
                else
                    gl.copyTexSubImage2D(gl.TEXTURE_2D, 0, 0, 0, 0, 0, cornerSize, cornerSize);
                break;
            case "blit-sub":
                clearSource(gl);
                gl.bindFramebuffer(gl.READ_FRAMEBUFFER, source.framebuffer);
                gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, target.framebuffer);
                gl.blitFramebuffer(0, 0, cornerSize, cornerSize, 0, 0, cornerSize, cornerSize, gl.COLOR_BUFFER_BIT, gl.NEAREST);
                break;
            case "read":
                break;
            }
        },
        probe(gl, n) {
            const [inside, outside] = probePositions(n);
            if (options.operation == "sample") {
                WebGLBench.probe(gl, sampleTarget, inside[0] % sampleTargetSize, inside[1] % sampleTargetSize, WebGLBench.color(n, 0), tolerance);
                WebGLBench.probe(gl, target, outside[0], outside[1], zero, 0);
                return;
            }
            WebGLBench.probe(gl, target, inside[0], inside[1], writes ? WebGLBench.color(n, 0) : zero, tolerance);
            WebGLBench.probe(gl, target, outside[0], outside[1], zero, 0);
        },
        verify(gl, n) {
            const check = { framebuffer: checkFramebuffer };
            for (let checkedLayer = 0; checkedLayer < (array ? layerCount : 1); ++checkedLayer) {
                const label = array ? "Layer " + checkedLayer : "Texture";
                attach(gl, checkFramebuffer, texture, checkedLayer);
                const writtenLayer = !array || checkedLayer == layer;
                const corner = WebGLBench.readPixels(gl, check, 0, 0, cornerSize, cornerSize);
                WebGLBench.verifyPixelsAreColor(label + " corner", corner, writes && writtenLayer ? WebGLBench.color(n, 0) : zero, cornerSize, tolerance);
                const right = WebGLBench.readPixels(gl, check, cornerSize, 0, size - cornerSize, size);
                WebGLBench.verifyPixelsAreColor(label + " right of the corner", right, zero, size - cornerSize, 0);
                const above = WebGLBench.readPixels(gl, check, 0, cornerSize, cornerSize, size - cornerSize);
                WebGLBench.verifyPixelsAreColor(label + " above the corner", above, zero, cornerSize, 0);
            }
            attach(gl, checkFramebuffer, null, 0);
            if (options.operation == "sample")
                WebGLBench.verifyTargetColor(gl, sampleTarget, "Sampled", WebGLBench.color(n, 0));
        }
    });
};
})();
