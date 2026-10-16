// Each run reallocates the storage of the renderbuffers attached to a framebuffer and clears them
// completely, then reads back one pixel. WebGL requires robust resource initialization, so new
// storage is uninitialized and an implementation may zero-fill it before the clear.
//
// The RenderbufferClear/*.html benchmarks, which generate-tests.py writes, call
// runRenderbufferClear() with each color-renderable renderbuffer format of WebGL 2 and
// EXT_color_buffer_float, alone and with a depth, stencil or depth-stencil renderbuffer,
// single-sampled and, for formats that allow it, multisampled. They also call it with each depth
// and stencil format alone.
(function () {
"use strict";

const size = 1024;
const sampleCount = 4;

// How readPixels() reads each type of color format, and the value of a channel that is 1.
const readFormats = {
    normalized: { format: "RGBA", type: "UNSIGNED_BYTE", array: Uint8Array, one: 255 },
    float: { format: "RGBA", type: "FLOAT", array: Float32Array, one: 1 },
    int: { format: "RGBA_INTEGER", type: "INT", array: Int32Array, one: 1 },
    uint: { format: "RGBA_INTEGER", type: "UNSIGNED_INT", array: Uint32Array, one: 1 },
};

// The values that iteration n clears to. Every channel, the depth and the stencil value change
// from one iteration to the next, and the color is exact in every format.
function colorValue(n)
{
    return [n % 2, (n + 1) % 2, n % 2, (n + 1) % 2];
}

function depthValue(n)
{
    return n % 2 ? 0.25 : 0.75;
}

function stencilValue(n)
{
    return 1 + n % 2;
}

const checkVertexShader = `
attribute vec2 aPosition;
uniform float uDepth;
void main() {
    gl_Position = vec4(aPosition, uDepth, 1.0);
}`;

const checkFragmentShader = `
precision mediump float;
void main() {
    gl_FragColor = vec4(1.0);
}`;

// options: { colorFormat, depthStencilFormat, multisample }. colorFormat or depthStencilFormat may
// be omitted, but not both. Their properties come from WebGLFormats, see
// WebGLBench.attachmentFormat(). A color format of type "normalized" or "float" is cleared with
// clear() and read back with readPixels() as RGBA UNSIGNED_BYTE or FLOAT, "int" and "uint" with
// clearBufferiv() or clearBufferuiv() and readPixels() as RGBA_INTEGER INT or UNSIGNED_INT. Read
// back, a missing green or blue channel is 0 and a missing alpha channel is 1.
window.runRenderbufferClear = function (options) {
    const color = options.colorFormat ? WebGLBench.attachmentFormat(options.colorFormat) : null;
    const depthStencil = options.depthStencilFormat ? WebGLBench.attachmentFormat(options.depthStencilFormat) : null;
    const read = color ? readFormats[color.type] : null;
    const samples = options.multisample ? sampleCount : 0;

    let framebuffer;
    let colorBuffer;
    let depthStencilBuffer;
    // A single-sampled renderbuffer of the color format that multisampled pixels are resolved to.
    let resolveFramebuffer;
    // The depth and stencil checks draw to an RGBA8 renderbuffer attached with the depth-stencil
    // renderbuffer, and read it back through checkResolveFramebuffer when multisampled.
    let checkFramebuffer;
    let checkResolveFramebuffer;
    let checkProgram;
    let checkDepthLocation;

    function allocate(gl, renderbuffer, format, sampleCount)
    {
        gl.bindRenderbuffer(gl.RENDERBUFFER, renderbuffer);
        if (sampleCount)
            gl.renderbufferStorageMultisample(gl.RENDERBUFFER, sampleCount, gl[format], size, size);
        else
            gl.renderbufferStorage(gl.RENDERBUFFER, gl[format], size, size);
    }

    function createFramebuffer(gl, attachments)
    {
        const result = gl.createFramebuffer();
        gl.bindFramebuffer(gl.FRAMEBUFFER, result);
        for (const [attachment, renderbuffer] of attachments)
            gl.framebufferRenderbuffer(gl.FRAMEBUFFER, attachment, gl.RENDERBUFFER, renderbuffer);
        const status = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
        if (status != gl.FRAMEBUFFER_COMPLETE)
            throw new Error("Framebuffer is not complete: 0x" + status.toString(16));
        return result;
    }

    function createRenderbuffer(gl, format, sampleCount)
    {
        const renderbuffer = gl.createRenderbuffer();
        allocate(gl, renderbuffer, format, sampleCount);
        return renderbuffer;
    }

    function expectSampleCount(gl, format)
    {
        const supported = gl.getInternalformatParameter(gl.RENDERBUFFER, gl[format], gl.SAMPLES);
        if (!supported || !supported.length || supported[0] < sampleCount)
            throw new Error(format + " does not support " + sampleCount + " samples");
    }

    // Reads the color of a rectangle of the framebuffer, resolving it first when multisampled.
    function readColor(gl, x, y, width, height)
    {
        let source = framebuffer;
        if (samples) {
            gl.bindFramebuffer(gl.READ_FRAMEBUFFER, framebuffer);
            gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, resolveFramebuffer);
            gl.blitFramebuffer(x, y, x + width, y + height, x, y, x + width, y + height, gl.COLOR_BUFFER_BIT, gl.NEAREST);
            source = resolveFramebuffer;
        }
        gl.bindFramebuffer(gl.FRAMEBUFFER, source);
        const pixels = new read.array(width * height * 4);
        gl.readPixels(x, y, width, height, gl[read.format], gl[read.type], pixels);
        return pixels;
    }

    function verifyColor(gl, n, x, y, width, height)
    {
        const value = colorValue(n);
        const pixel = [0, 1, 2, 3].map(c => c < color.channels ? value[c] * read.one : (c == 3 ? read.one : 0));
        WebGLBench.verifyPixelsAreColor("Color", readColor(gl, x, y, width, height), pixel, width, 0);
    }

    // Draws to pixels (0, 0) and (1, 0) of checkFramebuffer where the depth and stencil tests pass.
    // The stencil test passes where the stencil value is the cleared value. The depth test passes
    // at pixel 0 where the depth is greater than the cleared value - 0.125, and at pixel 1 where it
    // is less than the cleared value + 0.125.
    function verifyDepthStencil(gl, n)
    {
        gl.bindFramebuffer(gl.FRAMEBUFFER, checkFramebuffer);
        gl.useProgram(checkProgram);
        gl.enable(gl.SCISSOR_TEST);
        gl.scissor(0, 0, 2, 1);
        gl.clearColor(0, 0, 0, 0);
        gl.clear(gl.COLOR_BUFFER_BIT);
        gl.depthMask(false);
        if (depthStencil.depth)
            gl.enable(gl.DEPTH_TEST);
        if (depthStencil.stencil) {
            gl.enable(gl.STENCIL_TEST);
            gl.stencilFunc(gl.EQUAL, stencilValue(n), 0xff);
            gl.stencilOp(gl.KEEP, gl.KEEP, gl.KEEP);
        }
        // Window depth z is at clip space depth 2z - 1.
        gl.scissor(0, 0, 1, 1);
        gl.depthFunc(gl.LESS);
        gl.uniform1f(checkDepthLocation, 2 * (depthValue(n) - 0.125) - 1);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
        gl.scissor(1, 0, 1, 1);
        gl.depthFunc(gl.GREATER);
        gl.uniform1f(checkDepthLocation, 2 * (depthValue(n) + 0.125) - 1);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
        gl.disable(gl.SCISSOR_TEST);
        gl.disable(gl.DEPTH_TEST);
        gl.disable(gl.STENCIL_TEST);
        gl.depthFunc(gl.LESS);
        gl.depthMask(true);

        if (samples) {
            gl.bindFramebuffer(gl.READ_FRAMEBUFFER, checkFramebuffer);
            gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, checkResolveFramebuffer);
            gl.blitFramebuffer(0, 0, 2, 1, 0, 0, 2, 1, gl.COLOR_BUFFER_BIT, gl.NEAREST);
            gl.bindFramebuffer(gl.FRAMEBUFFER, checkResolveFramebuffer);
        }
        const pixels = new Uint8Array(8);
        gl.readPixels(0, 0, 2, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
        WebGLBench.verifyPixels("Depth and stencil tests against the cleared values", pixels, [255, 255, 255, 255, 255, 255, 255, 255], 2, 0);
    }

    const buffers = [];
    if (color)
        buffers.push(options.colorFormat + " renderbuffer");
    if (depthStencil)
        buffers.push(options.depthStencilFormat + " renderbuffer");
    const description = "Measures reallocating the storage of a " + size + "x" + size + " " + buffers.join(" and a ")
        + (samples ? ", " + samples + "x multisampled," : "") + " and clearing " + (buffers.length > 1 ? "them" : "it") + " completely, then reading back 1 pixel.";

    WebGLBench.run({
        description,
        contextType: "webgl2",
        extensions: [color, depthStencil].filter(format => format).flatMap(format => format.extensions),
        setup(gl) {
            if (samples) {
                if (color)
                    expectSampleCount(gl, options.colorFormat);
                if (depthStencil)
                    expectSampleCount(gl, options.depthStencilFormat);
            }

            const attachments = [];
            if (color) {
                colorBuffer = createRenderbuffer(gl, options.colorFormat, samples);
                attachments.push([gl.COLOR_ATTACHMENT0, colorBuffer]);
            }
            let depthStencilAttachment;
            if (depthStencil) {
                depthStencilBuffer = createRenderbuffer(gl, options.depthStencilFormat, samples);
                depthStencilAttachment = depthStencil.depth && depthStencil.stencil ? gl.DEPTH_STENCIL_ATTACHMENT
                    : depthStencil.depth ? gl.DEPTH_ATTACHMENT : gl.STENCIL_ATTACHMENT;
                attachments.push([depthStencilAttachment, depthStencilBuffer]);
            }
            framebuffer = createFramebuffer(gl, attachments);

            if (color && samples)
                resolveFramebuffer = createFramebuffer(gl, [[gl.COLOR_ATTACHMENT0, createRenderbuffer(gl, options.colorFormat, 0)]]);
            if (depthStencil) {
                checkFramebuffer = createFramebuffer(gl, [[gl.COLOR_ATTACHMENT0, createRenderbuffer(gl, "RGBA8", samples)], [depthStencilAttachment, depthStencilBuffer]]);
                if (samples)
                    checkResolveFramebuffer = createFramebuffer(gl, [[gl.COLOR_ATTACHMENT0, createRenderbuffer(gl, "RGBA8", 0)]]);
                checkProgram = WebGLBench.createProgram(gl, checkVertexShader, checkFragmentShader, ["aPosition"]);
                checkDepthLocation = gl.getUniformLocation(checkProgram, "uDepth");
                WebGLBench.bindFullscreenQuad(gl);
            }
            gl.viewport(0, 0, size, size);
        },
        iteration(gl, n) {
            gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
            if (color)
                allocate(gl, colorBuffer, options.colorFormat, samples);
            if (depthStencil)
                allocate(gl, depthStencilBuffer, options.depthStencilFormat, samples);

            let mask = 0;
            if (depthStencil && depthStencil.depth) {
                gl.clearDepth(depthValue(n));
                mask |= gl.DEPTH_BUFFER_BIT;
            }
            if (depthStencil && depthStencil.stencil) {
                gl.clearStencil(stencilValue(n));
                mask |= gl.STENCIL_BUFFER_BIT;
            }
            if (color) {
                const value = colorValue(n);
                if (color.type == "int")
                    gl.clearBufferiv(gl.COLOR, 0, value);
                else if (color.type == "uint")
                    gl.clearBufferuiv(gl.COLOR, 0, value);
                else {
                    gl.clearColor(value[0], value[1], value[2], value[3]);
                    mask |= gl.COLOR_BUFFER_BIT;
                }
            }
            if (mask)
                gl.clear(mask);
        },
        probe(gl, n) {
            if (color)
                verifyColor(gl, n, size - 1, size - 1, 1, 1);
            else
                verifyDepthStencil(gl, n);
        },
        verify(gl, n) {
            if (color)
                verifyColor(gl, n, 0, 0, size, size);
            if (depthStencil)
                verifyDepthStencil(gl, n);
        }
    });
};
})();
