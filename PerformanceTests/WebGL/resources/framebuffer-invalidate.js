// Each run is one frame of a WebGL 2 page that draws to its canvas: it checks the fences of the
// earlier frames without waiting, updates a uniform buffer, clears the canvas, draws a full-screen
// triangle to it that samples a canvas-sized RGBA16F texture, calls fenceSync() and flush(), then
// reads back 1 pixel of the canvas. Nothing ever draws into the sampled texture.
//
// With invalidateUnwrittenTexture, each frame first binds a framebuffer whose only attachment is
// another canvas-sized RGBA16F texture that nothing ever draws into, and invalidates its color
// attachment with invalidateFramebuffer(). Nothing else uses that framebuffer or its texture, so
// the frame should cost the same as without it.
//
// With drawDepthFirst as well, that framebuffer also has a depth texture, and before the
// invalidation the frame clears the depth and draws the triangle to it with color writes off, as a
// depth prepass does.
//
// Check: the sampled texture reads as zero, so the triangle's color is uTint, which changes with n.
// probe() reads 1 pixel of the canvas, at a position that changes with n. verify() reads the whole
// canvas.
(function () {
"use strict";

// The canvas size of the iPhone that the frame was first measured on.
const width = 603;
const height = 1032;
const fencesInFlight = 3;
const tolerance = 1;
// The bytes of the uniform buffer that each frame updates: a camera and 12 objects, each at an
// offset aligned to 256 bytes.
const uniformBufferSize = 256 * 13;

const vertexShader = `#version 300 es
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}`;

const fragmentShader = `#version 300 es
precision highp float;
precision highp sampler2D;
uniform sampler2D uSceneColor;
uniform vec3 uTint;
out vec4 outColor;
void main() {
    vec3 c = texelFetch(uSceneColor, ivec2(gl_FragCoord.xy), 0).rgb;
    c = c / (1.0 + c);
    outColor = vec4(pow(c, vec3(1.0 / 2.2)) + uTint, 1.0);
}`;

function createTexture(gl, unit)
{
    const texture = gl.createTexture();
    gl.activeTexture(gl.TEXTURE0 + unit);
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texStorage2D(gl.TEXTURE_2D, 1, gl.RGBA16F, width, height);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    return texture;
}

// A pixel of the canvas for run n.
function probePosition(n)
{
    const value = WebGLBench.hash(n);
    return [value % width, (value >>> 16) % height];
}

// options: { invalidateUnwrittenTexture, drawDepthFirst }.
window.runFramebufferInvalidate = function (options) {
    let tintLocation;
    let uniformBuffer;
    let uniforms;
    let unwrittenFramebuffer;
    const fences = [];
    const tint = [0, 0, 0, 0];

    const description = "Measures a frame that clears a " + width + "x" + height + " canvas, draws a full-screen triangle to it that samples an unwritten"
        + " RGBA16F texture, calls fenceSync() and flush(), and reads back 1 pixel"
        + (!options.invalidateUnwrittenTexture ? "."
            : options.drawDepthFirst ? ", after binding a framebuffer with another unwritten canvas-sized RGBA16F texture and a depth texture, clearing the depth, drawing the triangle to it with color writes off and invalidating its color attachment."
            : ", after binding a framebuffer whose only attachment is another unwritten canvas-sized RGBA16F texture and invalidating its color attachment.");

    WebGLBench.run({
        description,
        contextType: "webgl2",
        canvasSize: width,
        contextAttributes: { alpha: false, depth: false, stencil: false, antialias: false, preserveDrawingBuffer: false },
        extensions: ["EXT_color_buffer_float"],
        setup(gl) {
            gl.canvas.height = height;

            const program = WebGLBench.createProgram(gl, vertexShader, fragmentShader);
            tintLocation = gl.getUniformLocation(program, "uTint");
            gl.uniform1i(gl.getUniformLocation(program, "uSceneColor"), 4);
            createTexture(gl, 4);

            unwrittenFramebuffer = gl.createFramebuffer();
            gl.bindFramebuffer(gl.FRAMEBUFFER, unwrittenFramebuffer);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, createTexture(gl, 7), 0);
            if (options.drawDepthFirst) {
                const depth = gl.createTexture();
                gl.activeTexture(gl.TEXTURE6);
                gl.bindTexture(gl.TEXTURE_2D, depth);
                gl.texStorage2D(gl.TEXTURE_2D, 1, gl.DEPTH_COMPONENT32F, width, height);
                gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.TEXTURE_2D, depth, 0);
            }
            const status = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
            if (status != gl.FRAMEBUFFER_COMPLETE)
                throw new Error("Framebuffer is not complete: 0x" + status.toString(16));
            gl.bindFramebuffer(gl.FRAMEBUFFER, null);

            uniforms = new Float32Array(uniformBufferSize / 4);
            uniformBuffer = gl.createBuffer();
            gl.bindBuffer(gl.UNIFORM_BUFFER, uniformBuffer);
            gl.bufferData(gl.UNIFORM_BUFFER, uniformBufferSize, gl.DYNAMIC_DRAW);

            // The vertex shader makes the corners from gl_VertexID, and needs no attributes.
            gl.bindVertexArray(gl.createVertexArray());
        },
        iteration(gl, n) {
            while (fences.length && gl.getSyncParameter(fences[0], gl.SYNC_STATUS) == gl.SIGNALED)
                gl.deleteSync(fences.shift());
            while (fences.length >= fencesInFlight)
                gl.deleteSync(fences.shift());
            uniforms[0] = n;
            gl.bindBuffer(gl.UNIFORM_BUFFER, uniformBuffer);
            gl.bufferSubData(gl.UNIFORM_BUFFER, 0, uniforms);

            if (options.invalidateUnwrittenTexture) {
                gl.bindFramebuffer(gl.FRAMEBUFFER, unwrittenFramebuffer);
                if (options.drawDepthFirst) {
                    gl.viewport(0, 0, width, height);
                    gl.clear(gl.DEPTH_BUFFER_BIT);
                    gl.enable(gl.DEPTH_TEST);
                    gl.colorMask(false, false, false, false);
                    gl.drawArrays(gl.TRIANGLES, 0, 3);
                    gl.colorMask(true, true, true, true);
                    gl.disable(gl.DEPTH_TEST);
                }
                gl.invalidateFramebuffer(gl.DRAW_FRAMEBUFFER, [gl.COLOR_ATTACHMENT0]);
            }

            gl.bindFramebuffer(gl.FRAMEBUFFER, null);
            gl.viewport(0, 0, width, height);
            gl.clearColor(0, 0, 0, 1);
            gl.clear(gl.COLOR_BUFFER_BIT);
            WebGLBench.colorInto(n, 0, tint);
            gl.uniform3f(tintLocation, tint[0] / 255, tint[1] / 255, tint[2] / 255);
            gl.drawArrays(gl.TRIANGLES, 0, 3);
            fences.push(gl.fenceSync(gl.SYNC_GPU_COMMANDS_COMPLETE, 0));
            gl.flush();
        },
        probe(gl, n) {
            const [x, y] = probePosition(n);
            WebGLBench.probe(gl, { framebuffer: null }, x, y, WebGLBench.color(n, 0), tolerance);
        },
        verify(gl, n) {
            const pixels = WebGLBench.readPixels(gl, { framebuffer: null }, 0, 0, width, height);
            WebGLBench.verifyPixelsAreColor("Canvas", pixels, WebGLBench.color(n, 0), width, tolerance);
        }
    });
};
})();
