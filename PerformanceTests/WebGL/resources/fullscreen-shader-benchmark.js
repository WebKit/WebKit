// Shared driver for the shader evaluation benchmarks (category I): a fixed number of draws into a
// square RGBA8 target, followed by a 1x1 readPixels, checked against a JS reference.
//
// The default vertex shader draws a fullscreen quad and passes the pixel position to the fragment
// shader as the varying vPixel, which is the pixel center (x + 0.5, y + 0.5) at fragment centers.
// The shaders do not use gl_FragCoord.
//
// A tile-based GPU removes the fragments of opaque draws that a later opaque draw covers, so of K
// opaque fullscreen draws in a render pass only the last one would be shaded. Unless the test opts
// out, the target is cleared to zero, blending is additive, and the fragment shader's color is
// multiplied by the uniform uBenchOutputScale, which is 0 for every draw but the last and 1 for
// the last. The result is the last draw's color, but every draw reads the destination, so every
// draw is shaded. Blend constants are not used, because ANGLE's WebGPU backend ignores them.
//
// test: {
//     description,
//     size,                    // width and height of the target, 2048 by default
//     vertexShader,            // optional, FullscreenShaderBenchmark.vertexShader(size) by default
//     fragmentShader,
//     attributes,              // optional, ["aPosition"] by default
//     drawCount,               // draws per iteration
//     tolerance,               // per channel, for probe and verify
//     allowHiddenSurfaceRemoval, // optional; true leaves blending to the test, so covered draws may be skipped
//     sampleCount,             // optional, pixels checked by verify, 256 by default
//     bindGeometry(gl),        // optional, binds the vertex attributes; a fullscreen quad by default
//     setup(gl, program, target), // optional, after the program is linked and in use
//     clearColor(n),           // optional, RGBA bytes the target is cleared to before the draws;
//                              // only with allowHiddenSurfaceRemoval
//     update(gl, n),           // optional, before the draws of iteration n
//     draw(gl, n, i),          // optional, issues draw i; drawArrays(TRIANGLES, 0, 6) by default
//     expectedPixel(n, x, y, out), // fills out with RGBA bytes, or returns null when the pixel is
//                              // too close to a discontinuity to be checked
// }

(function () {
"use strict";

const FullscreenShaderBenchmark = {};

FullscreenShaderBenchmark.vertexShader = function (size) {
    return `
attribute vec2 aPosition;
varying highp vec2 vPixel;
void main() {
    vPixel = (aPosition * 0.5 + 0.5) * ${size}.0;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}`;
};

// A float in [0, 1) for (n, i), with 24 significant bits.
FullscreenShaderBenchmark.random = function (n, i) {
    return (WebGLBench.hash(n * 7919 + i * 104729 + 12345) >>> 8) / 16777216;
};

FullscreenShaderBenchmark.run = function (test) {
    const size = test.size || 2048;
    const tolerance = test.tolerance || 0;
    const sampleCount = test.sampleCount || 256;
    let target;

    const preventHiddenSurfaceRemoval = !test.allowHiddenSurfaceRemoval;
    if (preventHiddenSurfaceRemoval && test.clearColor)
        throw new Error("clearColor needs allowHiddenSurfaceRemoval");
    let description = test.description;
    if (preventHiddenSurfaceRemoval)
        description += " Additive blending of draws whose color is multiplied by zero, except the last, keeps the GPU from skipping covered draws.";
    let fragmentShader = test.fragmentShader;
    if (preventHiddenSurfaceRemoval) {
        const end = fragmentShader.lastIndexOf("}");
        fragmentShader = "uniform mediump float uBenchOutputScale;\n" + fragmentShader.substring(0, end)
            + "    gl_FragColor = gl_FragColor * uBenchOutputScale;\n" + fragmentShader.substring(end);
    }
    let outputScaleLocation;

    WebGLBench.run({
        description,
        setup(gl) {
            target = WebGLBench.createRenderTarget(gl, size, size);
            const program = WebGLBench.createProgram(gl, test.vertexShader || FullscreenShaderBenchmark.vertexShader(size), fragmentShader, test.attributes || ["aPosition"]);
            if (test.bindGeometry)
                test.bindGeometry(gl);
            else
                WebGLBench.bindFullscreenQuad(gl);
            if (preventHiddenSurfaceRemoval) {
                outputScaleLocation = gl.getUniformLocation(program, "uBenchOutputScale");
                gl.enable(gl.BLEND);
                gl.blendFunc(gl.ONE, gl.ONE);
            }
            if (test.setup)
                test.setup(gl, program, target);
        },
        iteration(gl, n) {
            if (test.clearColor) {
                const color = test.clearColor(n);
                gl.clearColor(color[0] / 255, color[1] / 255, color[2] / 255, color[3] / 255);
                gl.clear(gl.COLOR_BUFFER_BIT);
            }
            if (preventHiddenSurfaceRemoval) {
                gl.clearColor(0, 0, 0, 0);
                gl.clear(gl.COLOR_BUFFER_BIT);
                gl.uniform1f(outputScaleLocation, 0);
            }
            if (test.update)
                test.update(gl, n);
            for (let i = 0; i < test.drawCount; ++i) {
                if (preventHiddenSurfaceRemoval && i == test.drawCount - 1)
                    gl.uniform1f(outputScaleLocation, 1);
                if (test.draw)
                    test.draw(gl, n, i);
                else
                    gl.drawArrays(gl.TRIANGLES, 0, 6);
            }
        },
        probe(gl, n) {
            const expected = [0, 0, 0, 0];
            for (let attempt = 0; attempt < 256; ++attempt) {
                const x = WebGLBench.hash(n * 512 + attempt * 2) % size;
                const y = WebGLBench.hash(n * 512 + attempt * 2 + 1) % size;
                if (test.expectedPixel(n, x, y, expected)) {
                    WebGLBench.probe(gl, target, x, y, expected, tolerance);
                    return;
                }
            }
            throw new Error("No pixel of iteration " + n + " can be probed");
        },
        verify(gl, n) {
            const pixels = WebGLBench.readPixels(gl, target, 0, 0, size, size);
            const expected = [0, 0, 0, 0];
            let checked = 0;
            for (let i = 0; checked < sampleCount && i < sampleCount * 64; ++i) {
                const x = WebGLBench.hash(i * 2) % size;
                const y = WebGLBench.hash(i * 2 + 1) % size;
                if (!test.expectedPixel(n, x, y, expected))
                    continue;
                ++checked;
                WebGLBench.verifyPixels("Pixel (" + x + ", " + y + ")", pixels.subarray((y * size + x) * 4), expected, 1, tolerance);
            }
            if (checked < sampleCount)
                throw new Error("Only " + checked + " pixels of iteration " + n + " can be verified");
        }
    });
};

window.FullscreenShaderBenchmark = FullscreenShaderBenchmark;
})();
