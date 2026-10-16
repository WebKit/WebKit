// Shared parts of the draw call benchmarks. They draw into a 256x256 framebuffer split into 1024
// tiles of 8x8 pixels, one tile per draw. The color of a tile is the sum of a fixed color of the
// tile, from a vertex attribute, and a color of the iteration, usually from a uniform set once per
// iteration. Neither sum saturates, so every tile changes with every iteration, and a tile that a
// draw misses keeps the color of an earlier iteration.

const DrawCallBench = (function () {
"use strict";

const size = 256;
const tileSize = 8;
const tilesPerRow = size / tileSize;
const tileCount = tilesPerRow * tilesPerRow;

// Each vertex is a vec2 position in pixels as floats, the color of the tile as normalized bytes
// and a vec2 texture coordinate as normalized bytes.
const vertexSize = 16;

// The vertices of two triangles covering a tile, in pixels from the corner of the tile.
const quadCorners = [[0, 0], [tileSize, 0], [0, tileSize], [0, tileSize], [tileSize, 0], [tileSize, tileSize]];

const vertexShader = `
attribute vec2 aPosition;
attribute vec4 aColor;
varying vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = vec4(aPosition / ${size / 2}.0 - 1.0, 0.0, 1.0);
}`;

const fragmentShader = `
precision highp float;
uniform vec4 uColor;
varying vec4 vColor;
void main() {
    gl_FragColor = vec4(vColor.rgb + uColor.rgb, 1.0);
}`;

// The fixed color of tile i, with color channels below 2^bits and alpha from 64 to 191.
function tileColorInto(i, out, bits)
{
    const value = WebGLBench.hash(i + 0x10000);
    const shift = 8 - (bits || 7);
    out[0] = (value & 0xff) >> shift;
    out[1] = ((value >>> 8) & 0xff) >> shift;
    out[2] = ((value >>> 16) & 0xff) >> shift;
    out[3] = 64 + ((value >>> 24) & 0x7f);
    return out;
}

// A color of iteration n, with channels below 2^bits, to set as a uniform.
function iterationColorInto(n, i, out, bits)
{
    WebGLBench.colorInto(n, i, out);
    const shift = 8 - (bits || 7);
    out[0] >>= shift;
    out[1] >>= shift;
    out[2] >>= shift;
    return out;
}

function tileAt(x, y)
{
    return Math.floor(y / tileSize) * tilesPerRow + Math.floor(x / tileSize);
}

// The vertices of the given tiles, each with the corners given in pixels from the corner of the
// tile, in the vertex format above.
function createTileVertices(tiles, corners, bits)
{
    const vertices = new ArrayBuffer(tiles.length * corners.length * vertexSize);
    const positions = new Float32Array(vertices);
    const bytes = new Uint8Array(vertices);
    const color = [0, 0, 0, 0];
    tiles.forEach((tile, index) => {
        const tileX = (tile % tilesPerRow) * tileSize;
        const tileY = Math.floor(tile / tilesPerRow) * tileSize;
        tileColorInto(tile, color, bits);
        corners.forEach(([x, y], vertex) => {
            const offset = (index * corners.length + vertex) * vertexSize;
            positions[offset / 4] = tileX + x;
            positions[offset / 4 + 1] = tileY + y;
            bytes.set(color, offset + 8);
            bytes[offset + 12] = Math.round(x / tileSize * 255);
            bytes[offset + 13] = Math.round(y / tileSize * 255);
        });
    });
    return vertices;
}

function allTiles()
{
    return Array.from({ length: tileCount }, (value, i) => i);
}

// Creates a buffer with the vertices and points attributes 0 (position), 1 (color) and, if
// texCoords, 2 (texture coordinate) at it.
function bindTileVertices(gl, vertices, texCoords)
{
    const buffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, vertexSize, 0);
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 4, gl.UNSIGNED_BYTE, true, vertexSize, 8);
    if (texCoords) {
        gl.enableVertexAttribArray(2);
        gl.vertexAttribPointer(2, 2, gl.UNSIGNED_BYTE, true, vertexSize, 12);
    }
    return buffer;
}

return {
    size,
    tileSize,
    tilesPerRow,
    tileCount,
    quadCorners,
    vertexShader,
    fragmentShader,
    tileColorInto,
    iterationColorInto,
    tileAt,
    createTileVertices,
    allTiles,
    bindTileVertices,
};
})();

// Draws all tiles with the same program, buffers and uniforms: with drawArrays when indexType is
// null, and otherwise with drawElements and indices of that type.
function runSameStateBenchmark(indexType)
{
    const size = DrawCallBench.size;
    const tileSize = DrawCallBench.tileSize;
    const drawCount = DrawCallBench.tileCount;

    let target;
    let colorLocation;
    let glIndexType;
    let indexSize;
    const iterationColor = [0, 0, 0, 0];
    const tileColor = [0, 0, 0, 0];

    function expectedTileInto(n, i, out)
    {
        DrawCallBench.iterationColorInto(n, drawCount, iterationColor);
        DrawCallBench.tileColorInto(i, tileColor);
        for (let c = 0; c < 3; ++c)
            out[c] = tileColor[c] + iterationColor[c];
        out[3] = 255;
        return out;
    }

    let description;
    if (!indexType)
        description = "Measures 1024 drawArrays calls of 2 triangles each from one vertex buffer, with no state changes between them, followed by a 1x1 readPixels.";
    else
        description = "Measures 1024 drawElements calls of 2 triangles each with " + indexType + " indices from one index buffer, with no state changes between them, followed by a 1x1 readPixels.";

    WebGLBench.run({
        description,
        setup(gl) {
            target = WebGLBench.createRenderTarget(gl, size, size);
            const program = WebGLBench.createProgram(gl, DrawCallBench.vertexShader, DrawCallBench.fragmentShader, ["aPosition", "aColor"]);
            colorLocation = gl.getUniformLocation(program, "uColor");
            if (!indexType) {
                DrawCallBench.bindTileVertices(gl, DrawCallBench.createTileVertices(DrawCallBench.allTiles(), DrawCallBench.quadCorners));
                return;
            }

            // Four vertices and six indices per tile.
            DrawCallBench.bindTileVertices(gl, DrawCallBench.createTileVertices(DrawCallBench.allTiles(), [[0, 0], [tileSize, 0], [0, tileSize], [tileSize, tileSize]]));
            let indices;
            if (indexType == "UNSIGNED_INT") {
                if (!gl.getExtension("OES_element_index_uint"))
                    throw new Error("OES_element_index_uint is not supported");
                indices = new Uint32Array(drawCount * 6);
            } else
                indices = new Uint16Array(drawCount * 6);
            for (let i = 0; i < drawCount; ++i)
                indices.set([0, 1, 2, 2, 1, 3].map(index => i * 4 + index), i * 6);
            glIndexType = gl[indexType];
            indexSize = indices.BYTES_PER_ELEMENT;
            gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, gl.createBuffer());
            gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW);
        },
        iteration(gl, n) {
            DrawCallBench.iterationColorInto(n, drawCount, iterationColor);
            gl.uniform4f(colorLocation, iterationColor[0] / 255, iterationColor[1] / 255, iterationColor[2] / 255, 1);
            if (!indexType) {
                for (let i = 0; i < drawCount; ++i)
                    gl.drawArrays(gl.TRIANGLES, i * 6, 6);
                return;
            }
            for (let i = 0; i < drawCount; ++i)
                gl.drawElements(gl.TRIANGLES, 6, glIndexType, i * 6 * indexSize);
        },
        probe(gl, n) {
            WebGLBench.probe(gl, target, size - tileSize / 2, size - tileSize / 2, expectedTileInto(n, drawCount - 1, [0, 0, 0, 0]), 1);
        },
        verify(gl, n) {
            WebGLBench.verifyTarget(gl, target, "Tiles", 1, (x, y, out) => expectedTileInto(n, DrawCallBench.tileAt(x, y), out));
        }
    });
}
