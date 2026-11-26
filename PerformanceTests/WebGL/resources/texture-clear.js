// Each run creates a 2D texture with immutable storage, attaches it to a framebuffer, clears it
// completely and reads back one pixel. WebGL requires robust resource initialization, so new
// storage is uninitialized and an implementation may zero-fill it before the clear. That cost is
// what this benchmark measures.
//
// Each run makes a new texture rather than reallocating the storage of one texture, because a
// texture with immutable storage can't be reallocated, and texStorage2D() is how WebGL 2 content
// allocates textures that it renders to. A run deletes the texture of the previous run, calls
// createTexture(), texStorage2D() with one level, and framebufferTexture2D() on a framebuffer
// that exists for the whole benchmark, then clears. Deleting the texture of the previous run
// at the start of the next run, rather than at the end of its own, lets probe() read it back.
//
// The TextureClear/*.html benchmarks, which generate-tests.py writes, call runTextureClear() with
// each sized texture format that is color-renderable in WebGL 2, by itself or with the extensions
// EXT_color_buffer_float, EXT_texture_norm16, EXT_render_snorm or WEBGL_render_shared_exponent, and
// with each depth and depth-stencil texture format.
(function () {
"use strict";

const size = 1024;

// How readPixels() reads each type of color format, and the value of a channel that is 1.
const readFormats = {
    normalized: { format: "RGBA", type: "UNSIGNED_BYTE", array: Uint8Array, one: 255 },
    snorm8: { format: "RGBA", type: "BYTE", array: Int8Array, one: 127 },
    snorm16: { format: "RGBA", type: "SHORT", array: Int16Array, one: 32767 },
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

// options: { format }, a color format or a depth or depth-stencil format. Its properties come from
// WebGLFormats, see WebGLBench.attachmentFormat(). A color format of type "normalized" or "float"
// is cleared with clear() and read back with readPixels() as RGBA UNSIGNED_BYTE or FLOAT, "snorm8"
// and "snorm16" with clear() and readPixels() as RGBA BYTE or SHORT, "int" and "uint" with
// clearBufferiv() or clearBufferuiv() and readPixels() as RGBA_INTEGER INT or UNSIGNED_INT. Read
// back, a missing green or blue channel is 0 and a missing alpha channel is 1.
window.runTextureClear = function (options) {
    const properties = WebGLBench.attachmentFormat(options.format);
    const color = properties.type ? properties : null;
    const depthStencil = color ? null : properties;
    const read = color ? readFormats[color.type] : null;

    let internalFormat;
    let attachment;
    let framebuffer;
    let texture = null;
    // The depth and stencil checks draw to an RGBA8 texture attached with the depth texture.
    let checkFramebuffer;
    let checkProgram;
    let checkDepthLocation;

    function createFramebuffer(gl)
    {
        const result = gl.createFramebuffer();
        gl.bindFramebuffer(gl.FRAMEBUFFER, result);
        return result;
    }

    function expectComplete(gl)
    {
        const status = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
        if (status != gl.FRAMEBUFFER_COMPLETE)
            throw new Error(options.format + " framebuffer is not complete: 0x" + status.toString(16));
    }

    function createTexture(gl, format)
    {
        const result = gl.createTexture();
        gl.bindTexture(gl.TEXTURE_2D, result);
        gl.texStorage2D(gl.TEXTURE_2D, 1, format, size, size);
        gl.bindTexture(gl.TEXTURE_2D, null);
        return result;
    }

    function verifyColor(gl, n, x, y, width, height)
    {
        gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
        const pixels = new read.array(width * height * 4);
        gl.readPixels(x, y, width, height, gl[read.format], gl[read.type], pixels);
        const value = colorValue(n);
        const pixel = [0, 1, 2, 3].map(c => c < color.channels ? value[c] * read.one : (c == 3 ? read.one : 0));
        WebGLBench.verifyPixelsAreColor("Color", pixels, pixel, width, 0);
    }

    // Draws to a rectangle of checkFramebuffer where the depth and stencil tests pass. The stencil
    // test passes where the stencil value is the cleared value. The first draw writes red where
    // the depth is greater than the cleared value - 0.125, and the second writes green where it is
    // less than the cleared value + 0.125.
    function verifyDepthStencil(gl, n, x, y, width, height)
    {
        gl.bindFramebuffer(gl.FRAMEBUFFER, checkFramebuffer);
        gl.framebufferTexture2D(gl.FRAMEBUFFER, attachment, gl.TEXTURE_2D, texture, 0);
        gl.useProgram(checkProgram);
        gl.enable(gl.SCISSOR_TEST);
        gl.scissor(x, y, width, height);
        gl.clearColor(0, 0, 0, 0);
        gl.clear(gl.COLOR_BUFFER_BIT);
        gl.depthMask(false);
        gl.enable(gl.DEPTH_TEST);
        if (depthStencil.stencil) {
            gl.enable(gl.STENCIL_TEST);
            gl.stencilFunc(gl.EQUAL, stencilValue(n), 0xff);
            gl.stencilOp(gl.KEEP, gl.KEEP, gl.KEEP);
        }
        // Window depth z is at clip space depth 2z - 1.
        gl.colorMask(true, false, false, false);
        gl.depthFunc(gl.LESS);
        gl.uniform1f(checkDepthLocation, 2 * (depthValue(n) - 0.125) - 1);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
        gl.colorMask(false, true, false, false);
        gl.depthFunc(gl.GREATER);
        gl.uniform1f(checkDepthLocation, 2 * (depthValue(n) + 0.125) - 1);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
        gl.colorMask(true, true, true, true);
        gl.disable(gl.SCISSOR_TEST);
        gl.disable(gl.DEPTH_TEST);
        gl.disable(gl.STENCIL_TEST);
        gl.depthFunc(gl.LESS);
        gl.depthMask(true);

        const pixels = new Uint8Array(width * height * 4);
        gl.readPixels(x, y, width, height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
        // Detaches the texture, so that deleting it in the next run frees it.
        gl.framebufferTexture2D(gl.FRAMEBUFFER, attachment, gl.TEXTURE_2D, null, 0);
        WebGLBench.verifyPixelsAreColor("Depth and stencil tests against the cleared values", pixels, [255, 255, 0, 0], width, 0);
    }

    const description = "Measures creating a " + size + "x" + size + " " + options.format + " texture with texStorage2D, attaching it to a framebuffer and clearing it completely, then reading back 1 pixel.";

    WebGLBench.run({
        description,
        contextType: "webgl2",
        extensions: properties.extensions,
        setup(gl, extensions) {
            internalFormat = WebGLBench.enumValue(gl, extensions, options.format);

            if (color)
                attachment = gl.COLOR_ATTACHMENT0;
            else
                attachment = depthStencil.stencil ? gl.DEPTH_STENCIL_ATTACHMENT : gl.DEPTH_ATTACHMENT;
            texture = createTexture(gl, internalFormat);
            framebuffer = createFramebuffer(gl);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, attachment, gl.TEXTURE_2D, texture, 0);
            expectComplete(gl);

            if (depthStencil) {
                checkFramebuffer = createFramebuffer(gl);
                gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, createTexture(gl, gl.RGBA8), 0);
                gl.framebufferTexture2D(gl.FRAMEBUFFER, attachment, gl.TEXTURE_2D, texture, 0);
                expectComplete(gl);
                checkProgram = WebGLBench.createProgram(gl, checkVertexShader, checkFragmentShader, ["aPosition"]);
                checkDepthLocation = gl.getUniformLocation(checkProgram, "uDepth");
                WebGLBench.bindFullscreenQuad(gl);
            }
            gl.viewport(0, 0, size, size);
        },
        iteration(gl, n) {
            gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
            gl.deleteTexture(texture);
            texture = createTexture(gl, internalFormat);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, attachment, gl.TEXTURE_2D, texture, 0);

            if (depthStencil) {
                gl.clearDepth(depthValue(n));
                let mask = gl.DEPTH_BUFFER_BIT;
                if (depthStencil.stencil) {
                    gl.clearStencil(stencilValue(n));
                    mask |= gl.STENCIL_BUFFER_BIT;
                }
                gl.clear(mask);
                return;
            }
            const value = colorValue(n);
            if (color.type == "int")
                gl.clearBufferiv(gl.COLOR, 0, value);
            else if (color.type == "uint")
                gl.clearBufferuiv(gl.COLOR, 0, value);
            else {
                gl.clearColor(value[0], value[1], value[2], value[3]);
                gl.clear(gl.COLOR_BUFFER_BIT);
            }
        },
        probe(gl, n) {
            if (color)
                verifyColor(gl, n, size - 1, size - 1, 1, 1);
            else
                verifyDepthStencil(gl, n, size - 1, size - 1, 1, 1);
        },
        verify(gl, n) {
            if (color)
                verifyColor(gl, n, 0, 0, size, size);
            else
                verifyDepthStencil(gl, n, 0, 0, size, size);
        }
    });
};
})();
