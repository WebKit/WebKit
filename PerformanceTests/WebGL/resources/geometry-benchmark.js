// Shared parts of the indexed geometry benchmarks. They draw a grid of 1M triangles, 2 per cell of
// 2x1 pixels, over a 1024x1024 framebuffer, 4 times per iteration with additive blending. The
// fragment shader computes a pattern from the pixel position, which is interpolated from the
// vertex positions, plus a seed of the pass. The pattern and the seeds are below 64 per channel,
// so the sum of the 4 passes does not saturate and each pass changes the result.

const GeometryBench = (function () {
"use strict";

const size = 1024;
const columns = size / 2;
const rows = size;
const passCount = 4;

const vertexShader = `
attribute vec2 aPosition;
varying vec2 vPosition;
void main() {
    vPosition = aPosition;
    gl_Position = vec4(aPosition / ${size / 2}.0 - 1.0, 0.0, 1.0);
}`;

const fragmentShader = `
precision highp float;
uniform vec4 uSeed;
varying vec2 vPosition;
void main() {
    vec2 p = floor(vPosition);
    vec4 c = vec4(p, floor(p.x / 16.0) + floor(p.y / 16.0), 0.0) + uSeed;
    gl_FragColor = mod(c, 64.0) / 255.0;
}`;

// The vertex positions, in pixels, of rowCount rows of cells starting at firstRow.
function gridVertices(firstRow, rowCount)
{
    const vertices = new Float32Array((rowCount + 1) * (columns + 1) * 2);
    let offset = 0;
    for (let y = 0; y <= rowCount; ++y) {
        for (let x = 0; x <= columns; ++x) {
            vertices[offset++] = x * 2;
            vertices[offset++] = firstRow + y;
        }
    }
    return vertices;
}

// The indices of the triangles of rowCount rows of cells, in an array of ArrayType.
function gridIndices(rowCount, ArrayType)
{
    const indices = new ArrayType(rowCount * columns * 6);
    let offset = 0;
    for (let y = 0; y < rowCount; ++y) {
        for (let x = 0; x < columns; ++x) {
            const v00 = y * (columns + 1) + x;
            const v10 = v00 + 1;
            const v01 = v00 + columns + 1;
            const v11 = v01 + 1;
            indices[offset++] = v00;
            indices[offset++] = v11;
            indices[offset++] = v01;
            indices[offset++] = v00;
            indices[offset++] = v10;
            indices[offset++] = v11;
        }
    }
    return indices;
}

function seedInto(n, pass, out)
{
    const value = WebGLBench.hash(n * passCount + pass);
    for (let c = 0; c < 4; ++c)
        out[c] = (value >>> (c * 8)) & 63;
    return out;
}

function setSeed(gl, location, n, pass)
{
    const seed = seedInto(n, pass, [0, 0, 0, 0]);
    gl.uniform4f(location, seed[0], seed[1], seed[2], seed[3]);
}

// The target, with the clear color and the blending of the passes.
function createTarget(gl)
{
    const target = WebGLBench.createRenderTarget(gl, size, size);
    gl.clearColor(0, 0, 0, 0);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE);
    return target;
}

const seeds = Array.from({ length: passCount }, () => [0, 0, 0, 0]);

function expectedPixel(n, x, y, out)
{
    const pattern = [x, y, (x >> 4) + (y >> 4), 0];
    out[0] = out[1] = out[2] = out[3] = 0;
    for (let pass = 0; pass < passCount; ++pass) {
        seedInto(n, pass, seeds[pass]);
        for (let c = 0; c < 4; ++c)
            out[c] += (pattern[c] + seeds[pass][c]) & 63;
    }
    return out;
}

function probe(gl, target, n)
{
    const x = WebGLBench.hash(n) % size;
    const y = WebGLBench.hash(n + 1) % size;
    WebGLBench.probe(gl, target, x, y, expectedPixel(n, x, y, [0, 0, 0, 0]), 1);
}

function verify(gl, target, n)
{
    WebGLBench.verifyTarget(gl, target, "Grid", 1, (x, y, out) => expectedPixel(n, x, y, out));
}

return {
    size,
    columns,
    rows,
    passCount,
    vertexShader,
    fragmentShader,
    gridVertices,
    gridIndices,
    setSeed,
    createTarget,
    probe,
    verify,
};
})();
