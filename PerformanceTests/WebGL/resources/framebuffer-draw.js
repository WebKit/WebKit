// Each run reallocates the storage of the attachments of a framebuffer and draws to half of them,
// then reads back two pixels. The draw is the first command after the reallocation: there is no
// clear and no read of the new storage before it, in setup() or in any run. WebGL requires robust
// resource initialization, so before the draw the implementation must initialize the attachments:
// color to 0, depth to 1 and stencil to 0. That cost is what this benchmark measures.
// RenderbufferClear measures reallocating renderbuffers followed by a full clear.
//
// The target says what the framebuffer draws to:
//
//  - "rb": renderbuffers, which each run reallocates with renderbufferStorage() or
//    renderbufferStorageMultisample().
//  - "tex": 2D textures, color and depth-stencil both, which each run reallocates with
//    texImage2D() and no data, on the same textures. WebGL 2 has no multisampled textures, so a
//    multisampled "tex" benchmark draws to multisampled renderbuffers, as "rb" does, and resolves
//    them into textures.
//
// Multisampled benchmarks resolve the whole framebuffer after the draw, with one blitFramebuffer()
// of color, depth and stencil, whichever are attached, into a framebuffer of the same size with
// single-sampled attachments of the same formats: renderbuffers for "rb" and textures for "tex".
// Then they invalidate the attachments of the multisampled framebuffer. The resolve and the
// invalidation are part of each run. setup() allocates the resolve attachments once. Under robust
// resource initialization, the implementation initializes them before the first resolve, in the
// first run before measurement. Later resolves overwrite them completely, and they stay
// initialized, so the runs that are measured don't initialize them.
//
// Iteration n draws to the left half for even n and to the right half for odd n, so the half that
// was drawn in the previous run must read back as initialized. The draw writes a color, a depth and
// a stencil value that change from one iteration to the next, with the depth test and the stencil
// test enabled and passing. probe() reads back the two pixels on either side of the edge of the
// drawn half, one drawn and one initialized. verify() checks every pixel, the color by reading it
// back and the depth and stencil values by draws with depth and stencil tests. Both read only from
// the single-sampled attachments: the resolved ones when multisampled.
//
// The FramebufferDraw/*.html benchmarks, which generate-tests.py writes, call
// runFramebufferDraw() with each target, and the same formats as RenderbufferClear: each
// color-renderable format of WebGL 2 and EXT_color_buffer_float, alone and with a depth, stencil
// or depth-stencil attachment, single-sampled and, for formats that allow it, multisampled, and
// each depth and stencil format alone. "tex" leaves out STENCIL_INDEX8, which is not a texture
// format in WebGL 2.
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

// The values that robust resource initialization gives new storage, which the WebGL 2
// specification requires and ANGLE implements: color 0, depth 1 and stencil 0.
const initialColor = [0, 0, 0, 0];
const initialDepth = 1;
const initialStencil = 0;

// The values that iteration n draws. Every channel, the depth and the stencil value change from
// one iteration to the next, and the color is exact in every format.
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

// Iteration n draws to the columns from drawnX(n) to drawnX(n) + size / 2.
function drawnX(n)
{
    return n % 2 ? size / 2 : 0;
}

function undrawnX(n)
{
    return n % 2 ? 0 : size / 2;
}

const drawVertexShader = `#version 300 es
in vec2 aPosition;
uniform float uDepth;
void main() {
    gl_Position = vec4(aPosition, uDepth, 1.0);
}`;

// Integer formats need an integer output.
function drawFragmentShader(colorType)
{
    const prefix = { int: "i", uint: "u" }[colorType] || "";
    return `#version 300 es
precision highp float;
precision highp int;
uniform highp ${prefix}vec4 uColor;
out highp ${prefix}vec4 outColor;
void main() {
    outColor = uColor;
}`;
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

// options: { target, colorFormat, depthStencilFormat, multisample }. target is "rb" or "tex", see
// the comment at the top. colorFormat or depthStencilFormat may be omitted, but not both. Their
// properties come from WebGLFormats, see WebGLBench.attachmentFormat(). A color format of type
// "normalized" or "float" is drawn to with a vec4 output and read back with readPixels() as RGBA
// UNSIGNED_BYTE or FLOAT, "int" and "uint" with an ivec4 or uvec4 output and readPixels() as
// RGBA_INTEGER INT or UNSIGNED_INT. Read back, a missing green or blue channel is 0 and a missing
// alpha channel is 1.
window.runFramebufferDraw = function (options) {
    const color = options.colorFormat ? WebGLBench.attachmentFormat(options.colorFormat) : null;
    const depthStencil = options.depthStencilFormat ? WebGLBench.attachmentFormat(options.depthStencilFormat) : null;
    const read = color ? readFormats[color.type] : null;
    const samples = options.multisample ? sampleCount : 0;
    const textures = options.target == "tex";

    let gl;
    let depthStencilAttachment;
    // The framebuffer that each run draws to, and its color and depth-stencil attachments:
    // renderbuffers, or textures for single-sampled "tex" benchmarks.
    let drawFramebuffer;
    let drawColor;
    let drawDepthStencil;
    // When multisampled, the framebuffer that the draw is resolved to. Otherwise drawFramebuffer.
    let resultFramebuffer;
    let resultDepthStencil;
    let resolveMask = 0;
    let invalidatedAttachments = [];
    let drawProgram;
    let drawColorLocation;
    let drawDepthLocation;
    // Two quads, one for each half of the target.
    let drawVertexArray;
    // The depth and stencil checks draw to an RGBA8 renderbuffer attached with the single-sampled
    // depth-stencil attachment.
    let checkFramebuffer;
    let checkProgram;
    let checkDepthLocation;
    let checkVertexArray;

    // Allocates the storage of a renderbuffer or texture, which may already have storage.
    function allocate(object, format, sampleCount)
    {
        if (object instanceof WebGLTexture) {
            const properties = WebGLBench.textureFormat(format);
            gl.bindTexture(gl.TEXTURE_2D, object);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl[format], size, size, 0, gl[properties.format], gl[properties.type], null);
            gl.bindTexture(gl.TEXTURE_2D, null);
            return;
        }
        gl.bindRenderbuffer(gl.RENDERBUFFER, object);
        if (sampleCount)
            gl.renderbufferStorageMultisample(gl.RENDERBUFFER, sampleCount, gl[format], size, size);
        else
            gl.renderbufferStorage(gl.RENDERBUFFER, gl[format], size, size);
    }

    function create(isTexture, format, sampleCount)
    {
        const object = isTexture ? gl.createTexture() : gl.createRenderbuffer();
        allocate(object, format, sampleCount);
        return object;
    }

    function createFramebuffer(attachments)
    {
        const result = gl.createFramebuffer();
        gl.bindFramebuffer(gl.FRAMEBUFFER, result);
        for (const [attachment, object] of attachments) {
            if (object instanceof WebGLTexture)
                gl.framebufferTexture2D(gl.FRAMEBUFFER, attachment, gl.TEXTURE_2D, object, 0);
            else
                gl.framebufferRenderbuffer(gl.FRAMEBUFFER, attachment, gl.RENDERBUFFER, object);
        }
        const status = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
        if (status != gl.FRAMEBUFFER_COMPLETE)
            throw new Error("Framebuffer is not complete: 0x" + status.toString(16));
        return result;
    }

    function expectSampleCount(format)
    {
        const supported = gl.getInternalformatParameter(gl.RENDERBUFFER, gl[format], gl.SAMPLES);
        if (!supported || !supported.length || supported[0] < sampleCount)
            throw new Error(format + " does not support " + sampleCount + " samples");
    }

    function createVertexArray(vertices)
    {
        const vertexArray = gl.createVertexArray();
        gl.bindVertexArray(vertexArray);
        gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.STATIC_DRAW);
        gl.enableVertexAttribArray(0);
        gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
        gl.bindVertexArray(null);
        return vertexArray;
    }

    function readColor(x, y, width, height)
    {
        gl.bindFramebuffer(gl.FRAMEBUFFER, resultFramebuffer);
        const pixels = new read.array(width * height * 4);
        gl.readPixels(x, y, width, height, gl[read.format], gl[read.type], pixels);
        return pixels;
    }

    // The color that a pixel with value reads back as.
    function readValue(value)
    {
        return [0, 1, 2, 3].map(c => c < color.channels ? value[c] * read.one : (c == 3 ? read.one : 0));
    }

    // Draws to the rectangle of each region of checkFramebuffer where the depth and stencil tests
    // pass. The stencil test passes where the stencil value is the region's stencil. The first draw
    // writes red where the depth is greater than the region's depth - 0.125, and the second writes
    // green where it is less than the region's depth + 0.125. Depth values are at most 1, so the
    // second draw has no depth test when the region's depth + 0.125 is greater than 1, which would
    // also be clipped. Then reads back the rectangle from (x, y) to (x + width, y + height), which
    // must be the union of the regions, and checks that both draws wrote to every pixel.
    function verifyDepthStencil(label, regions, x, y, width, height)
    {
        gl.bindFramebuffer(gl.FRAMEBUFFER, checkFramebuffer);
        gl.useProgram(checkProgram);
        gl.bindVertexArray(checkVertexArray);
        gl.enable(gl.SCISSOR_TEST);
        gl.depthMask(false);
        if (depthStencil.depth)
            gl.enable(gl.DEPTH_TEST);
        if (depthStencil.stencil) {
            gl.enable(gl.STENCIL_TEST);
            gl.stencilOp(gl.KEEP, gl.KEEP, gl.KEEP);
        }
        gl.clearColor(0, 0, 0, 0);
        for (const region of regions) {
            gl.scissor(region.x, region.y, region.width, region.height);
            gl.clear(gl.COLOR_BUFFER_BIT);
            gl.stencilFunc(gl.EQUAL, region.stencil, 0xff);
            // Window depth z is at clip space depth 2z - 1.
            gl.colorMask(true, false, false, false);
            gl.depthFunc(gl.LESS);
            gl.uniform1f(checkDepthLocation, 2 * (region.depth - 0.125) - 1);
            gl.drawArrays(gl.TRIANGLES, 0, 6);
            gl.colorMask(false, true, false, false);
            if (region.depth + 0.125 > 1) {
                gl.depthFunc(gl.ALWAYS);
                gl.uniform1f(checkDepthLocation, 0);
            } else {
                gl.depthFunc(gl.GREATER);
                gl.uniform1f(checkDepthLocation, 2 * (region.depth + 0.125) - 1);
            }
            gl.drawArrays(gl.TRIANGLES, 0, 6);
            gl.colorMask(true, true, true, true);
        }
        gl.disable(gl.SCISSOR_TEST);
        gl.disable(gl.DEPTH_TEST);
        gl.disable(gl.STENCIL_TEST);
        gl.depthFunc(gl.LESS);
        gl.depthMask(true);

        const pixels = new Uint8Array(width * height * 4);
        gl.readPixels(x, y, width, height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
        WebGLBench.verifyPixelsAreColor(label, pixels, [255, 255, 0, 0], width, 0);
    }

    // The drawn and the initialized region of the columns from x to x + width and the rows from y
    // to y + height, with their depth and stencil values.
    function depthStencilRegions(n, x, y, width, height)
    {
        const drawn = { x: Math.max(x, drawnX(n)), y, height, depth: depthValue(n), stencil: stencilValue(n) };
        drawn.width = Math.min(x + width, drawnX(n) + size / 2) - drawn.x;
        const initialized = { x: Math.max(x, undrawnX(n)), y, height, depth: initialDepth, stencil: initialStencil };
        initialized.width = Math.min(x + width, undrawnX(n) + size / 2) - initialized.x;
        return [drawn, initialized].filter(region => region.width > 0);
    }

    // For example "an RGBA8 renderbuffer and a DEPTH24_STENCIL8 renderbuffer".
    const formats = [options.colorFormat, options.depthStencilFormat].filter(format => format);
    const list = kind => formats.map(format => format + " " + kind).join(" and a ");
    const them = formats.length > 1 ? "them" : "it";
    const they = formats.length > 1 ? "they" : "it";
    let description;
    if (samples) {
        description = "Measures reallocating the storage of a " + size + "x" + size + " " + samples + "x multisampled " + list("renderbuffer")
            + ", drawing to half of " + them + " with no clear before, so that " + they + " must be initialized first, resolving " + them
            + " into a " + list(textures ? "texture" : "renderbuffer") + " with blitFramebuffer and invalidating " + them + ", then reading back 2 pixels.";
    } else {
        description = "Measures reallocating the storage of a " + size + "x" + size + " " + list(textures ? "texture" : "renderbuffer") + " and drawing to half of "
            + them + " with no clear before, so that " + they + " must be initialized first, then reading back 2 pixels.";
    }

    WebGLBench.run({
        description,
        contextType: "webgl2",
        extensions: [color, depthStencil].filter(format => format).flatMap(format => format.extensions),
        setup(context) {
            gl = context;
            if (samples) {
                if (color)
                    expectSampleCount(options.colorFormat);
                if (depthStencil)
                    expectSampleCount(options.depthStencilFormat);
            }
            if (depthStencil) {
                depthStencilAttachment = depthStencil.depth && depthStencil.stencil ? gl.DEPTH_STENCIL_ATTACHMENT
                    : depthStencil.depth ? gl.DEPTH_ATTACHMENT : gl.STENCIL_ATTACHMENT;
            }

            // Only creates the objects and allocates storage: no clear or read of the attachments.
            const drawTextures = textures && !samples;
            const drawAttachments = [];
            if (color) {
                drawColor = create(drawTextures, options.colorFormat, samples);
                drawAttachments.push([gl.COLOR_ATTACHMENT0, drawColor]);
                resolveMask |= gl.COLOR_BUFFER_BIT;
                invalidatedAttachments.push(gl.COLOR_ATTACHMENT0);
            }
            if (depthStencil) {
                drawDepthStencil = create(drawTextures, options.depthStencilFormat, samples);
                drawAttachments.push([depthStencilAttachment, drawDepthStencil]);
                resolveMask |= (depthStencil.depth ? gl.DEPTH_BUFFER_BIT : 0) | (depthStencil.stencil ? gl.STENCIL_BUFFER_BIT : 0);
                invalidatedAttachments.push(depthStencilAttachment);
            }
            drawFramebuffer = createFramebuffer(drawAttachments);
            resultFramebuffer = drawFramebuffer;
            resultDepthStencil = drawDepthStencil;

            if (samples) {
                const resolveAttachments = [];
                if (color)
                    resolveAttachments.push([gl.COLOR_ATTACHMENT0, create(textures, options.colorFormat, 0)]);
                if (depthStencil) {
                    resultDepthStencil = create(textures, options.depthStencilFormat, 0);
                    resolveAttachments.push([depthStencilAttachment, resultDepthStencil]);
                }
                resultFramebuffer = createFramebuffer(resolveAttachments);
            }

            drawProgram = WebGLBench.createProgram(gl, drawVertexShader, drawFragmentShader(color ? color.type : "float"), ["aPosition"]);
            drawColorLocation = gl.getUniformLocation(drawProgram, "uColor");
            drawDepthLocation = gl.getUniformLocation(drawProgram, "uDepth");
            drawVertexArray = createVertexArray([
                -1, -1, 0, -1, -1, 1, -1, 1, 0, -1, 0, 1,
                0, -1, 1, -1, 0, 1, 0, 1, 1, -1, 1, 1,
            ]);

            if (depthStencil) {
                checkFramebuffer = createFramebuffer([[gl.COLOR_ATTACHMENT0, create(false, "RGBA8", 0)], [depthStencilAttachment, resultDepthStencil]]);
                checkProgram = WebGLBench.createProgram(gl, checkVertexShader, checkFragmentShader, ["aPosition"]);
                checkDepthLocation = gl.getUniformLocation(checkProgram, "uDepth");
                checkVertexArray = createVertexArray([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]);
            }
            gl.disable(gl.DITHER);
            gl.viewport(0, 0, size, size);
        },
        iteration(context, n) {
            gl.bindFramebuffer(gl.FRAMEBUFFER, drawFramebuffer);
            if (color)
                allocate(drawColor, options.colorFormat, samples);
            if (depthStencil)
                allocate(drawDepthStencil, options.depthStencilFormat, samples);

            gl.useProgram(drawProgram);
            gl.bindVertexArray(drawVertexArray);
            if (color) {
                const value = colorValue(n);
                if (color.type == "int")
                    gl.uniform4iv(drawColorLocation, value);
                else if (color.type == "uint")
                    gl.uniform4uiv(drawColorLocation, value);
                else
                    gl.uniform4fv(drawColorLocation, value);
            }
            if (depthStencil && depthStencil.depth) {
                gl.enable(gl.DEPTH_TEST);
                gl.depthFunc(gl.ALWAYS);
            }
            if (depthStencil && depthStencil.stencil) {
                gl.enable(gl.STENCIL_TEST);
                gl.stencilFunc(gl.ALWAYS, stencilValue(n), 0xff);
                gl.stencilOp(gl.KEEP, gl.KEEP, gl.REPLACE);
            }
            // Window depth z is at clip space depth 2z - 1.
            gl.uniform1f(drawDepthLocation, 2 * depthValue(n) - 1);
            gl.drawArrays(gl.TRIANGLES, n % 2 ? 6 : 0, 6);
            gl.disable(gl.DEPTH_TEST);
            gl.disable(gl.STENCIL_TEST);
            gl.depthFunc(gl.LESS);

            if (samples) {
                gl.bindFramebuffer(gl.READ_FRAMEBUFFER, drawFramebuffer);
                gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, resultFramebuffer);
                gl.blitFramebuffer(0, 0, size, size, 0, 0, size, size, resolveMask, gl.NEAREST);
                gl.invalidateFramebuffer(gl.READ_FRAMEBUFFER, invalidatedAttachments);
            }
        },
        probe(context, n) {
            // The last column of the left half and the first of the right half, in the top row.
            const x = size / 2 - 1;
            const y = size - 1;
            if (color) {
                const pixels = readColor(x, y, 2, 1);
                const left = readValue(n % 2 ? initialColor : colorValue(n));
                const right = readValue(n % 2 ? colorValue(n) : initialColor);
                WebGLBench.verifyPixels("Drawn and initialized color at the edge of the drawn half", pixels, left.concat(right), 2, 0);
            } else
                verifyDepthStencil("Depth and stencil tests against the drawn and initialized values at the edge of the drawn half", depthStencilRegions(n, x, y, 2, 1), x, y, 2, 1);
        },
        verify(context, n) {
            if (color) {
                WebGLBench.verifyPixelsAreColor("Drawn color", readColor(drawnX(n), 0, size / 2, size), readValue(colorValue(n)), size / 2, 0);
                WebGLBench.verifyPixelsAreColor("Initialized color", readColor(undrawnX(n), 0, size / 2, size), readValue(initialColor), size / 2, 0);
            }
            if (depthStencil)
                verifyDepthStencil("Depth and stencil tests against the drawn and initialized values", depthStencilRegions(n, 0, 0, size, size), 0, 0, size, size);
        }
    });
};
})();
