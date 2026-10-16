// Each run clears the default framebuffer and draws layerCount full-screen opaque quads to it back
// to front, with the depth test on, then reads back 5 pixels. Every quad passes the depth test when
// it arrives, so only the GPU's hidden surface removal can skip shading the quads that a later one
// covers. The fragment shaders cost about the same and differ in what may stop that removal:
//
//  - lod: reads a texture with textureLod() at level 0, so it needs no derivatives.
//  - texture: reads the texture with texture() at its implicit level, which needs derivatives.
//  - fwidth: reads the texture with textureLod() at level 0, and uses fwidth().
//  - discard: reads the texture with textureLod() at level 0, and has a discard that is never taken.
//  - notexture: reads no texture. It computes the same noise from the texel coordinate.
//
// The msaa benchmarks draw to a 4x multisampled default framebuffer (antialias: true).
//
// Check: a quad is a triangle strip of two triangles, whose shared edge is the diagonal from the
// bottom-right to the top-left corner. Next to that edge, a GPU that runs the two triangles'
// fragments together can compute derivatives wrongly, and texture() then reads the wrong mip level.
// setup() draws a reference of the top quad alone, split along the other diagonal so that the
// checked pixels are inside a triangle, and reads it back. probe() reads a row of 5 pixels across
// the shared edge, on a row that changes with n, and compares it with the reference. verify()
// compares 5 pixels across the edge on every row. Both skip the rows near the center, where the
// reference's own shared edge crosses the checked one.
(function () {
"use strict";

const size = 2048;
const layerCount = 64;
const noiseIterations = 32;
const textureSize = 256;
// The texture repeats this many times across the quad, so a texel covers about one pixel.
const textureRepeat = 8;
const stripWidth = 5;
const tolerance = 3;
// The rows that the checks skip, around the center.
const centerRows = 16;
const firstCenterRow = size / 2 - centerRows / 2;

const vertexShader = `#version 300 es
uniform float uDepth;
uniform bool uOtherDiagonal;
out vec2 vTexCoord;
void main() {
    vec2 corner = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));
    // Mirroring x moves the strip's shared edge to the other diagonal.
    if (uOtherDiagonal)
        corner.x = 1.0 - corner.x;
    vTexCoord = corner;
    gl_Position = vec4(corner * 2.0 - 1.0, uDepth, 1.0);
}`;

// The bytes of the texture's level 0: byte i is the top 8 bits of i * 2654435761.
function textureData()
{
    const data = new Uint8Array(textureSize * textureSize * 4);
    for (let i = 0; i < data.length; ++i)
        data[i] = Math.imul(i, 2654435761) >>> 24;
    return data;
}

function fragmentShader(shader)
{
    const coordinate = `vTexCoord * ${textureRepeat.toFixed(1)}`;
    const read = {
        lod: `textureLod(uTexture, ${coordinate}, 0.0).rgb`,
        texture: `texture(uTexture, ${coordinate}).rgb`,
        fwidth: `textureLod(uTexture, ${coordinate}, 0.0).rgb`,
        discard: `textureLod(uTexture, ${coordinate}, 0.0).rgb`,
        notexture: `noise(${coordinate})`,
    }[shader];
    const edge = shader == "fwidth" ? "fwidth(vTexCoord.x) * 64.0" : "vTexCoord.y * 0.001";
    return `#version 300 es
precision highp float;
precision highp int;
uniform highp sampler2D uTexture;
uniform vec3 uTint;
in vec2 vTexCoord;
out vec4 outColor;
// The texel of textureData() at a texture coordinate, as textureLod() at level 0 reads it at the
// pixel centers, where each pixel is at a texel center.
vec3 noise(vec2 coordinate) {
    uvec2 texel = uvec2(floor(coordinate * ${textureSize.toFixed(1)})) % ${textureSize}u;
    uint byteIndex = (texel.y * ${textureSize}u + texel.x) * 4u;
    return vec3(uvec3(byteIndex, byteIndex + 1u, byteIndex + 2u) * 2654435761u >> 24u) / 255.0;
}
void main() {
    vec3 color = ${read} * uTint;
    float edge = ${edge};
    for (int i = 0; i < ${noiseIterations}; i++)
        color = fract(sin(color * 1.731 + vec3(edge + float(i) * 0.013)) * 0.5 + 0.5);
    ${shader == "discard" ? "if (color.r > uTint.r + 1.5)\n        discard;" : ""}
    outColor = vec4(mix(uTint, color, 0.25), 1.0);
}`;
}

// Back to front: each quad is nearer than the one before it.
function layerDepth(layer)
{
    return 0.9 - 1.8 * layer / (layerCount - 1);
}

function tint(index)
{
    return [0.4 + 0.6 * ((index * 37) % 64) / 64, 0.4 + 0.6 * ((index * 11) % 64) / 64, 0.4 + 0.6 * ((index * 53) % 64) / 64];
}

// The top quad's tint alternates with n, so a result left from the previous run fails the check.
// The hidden quads' tints change with n too.
function layerTint(layer, n)
{
    if (layer == layerCount - 1)
        return tint(n % 2 ? 60 : 61);
    return tint((layer * 7 + n) % 59);
}

// The first pixel of the strip across the shared edge on row y. The edge runs from (size, 0) to
// (0, size), so on row y it is at x = size - 1 - y.
function stripStart(y)
{
    return Math.min(Math.max(size - 1 - y - (stripWidth >> 1), 0), size - stripWidth);
}

function isCenterRow(y)
{
    return y >= firstCenterRow && y < firstCenterRow + centerRows;
}

// A row outside the center rows, for run n.
function probeRow(n)
{
    const y = WebGLBench.hash(n) % (size - centerRows);
    return y < firstCenterRow ? y : y + centerRows;
}

// options: { shader, multisample }.
window.runHiddenSurfaceRemoval = function (options) {
    const attributes = { alpha: false, depth: true, stencil: false, antialias: options.multisample, preserveDrawingBuffer: false };
    let depthLocation;
    let tintLocation;
    let otherDiagonalLocation;
    // The reference image for even and odd n.
    const references = [];

    function draw(gl, n, layers, otherDiagonal)
    {
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.clearColor(0, 0, 0, 1);
        gl.clearDepth(1);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        gl.uniform1i(otherDiagonalLocation, otherDiagonal ? 1 : 0);
        for (const layer of layers) {
            gl.uniform1f(depthLocation, layerDepth(layer));
            gl.uniform3fv(tintLocation, layerTint(layer, n));
            gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
        }
    }

    function expectedStrip(n, y)
    {
        const start = (y * size + stripStart(y)) * 4;
        return references[n % 2].subarray(start, start + stripWidth * 4);
    }

    const description = "Measures clearing a " + size + "x" + size + (options.multisample ? " 4x multisampled" : "")
        + " default framebuffer and drawing " + layerCount + " full-screen opaque quads to it back to front with the depth test on,"
        + " with a fragment shader that " + {
            lod: "reads a texture with textureLod()",
            texture: "reads a texture with texture()",
            fwidth: "reads a texture with textureLod() and uses fwidth()",
            discard: "reads a texture with textureLod() and has a discard that is never taken",
            notexture: "computes noise without a texture",
        }[options.shader] + ", then reading back 5 pixels.";

    WebGLBench.run({
        description,
        contextType: "webgl2",
        canvasSize: size,
        contextAttributes: attributes,
        setup(gl) {
            const program = WebGLBench.createProgram(gl, vertexShader, fragmentShader(options.shader));
            depthLocation = gl.getUniformLocation(program, "uDepth");
            tintLocation = gl.getUniformLocation(program, "uTint");
            otherDiagonalLocation = gl.getUniformLocation(program, "uOtherDiagonal");

            // A mipmapped texture, so that texture() picks a level from the derivatives.
            gl.activeTexture(gl.TEXTURE0);
            gl.bindTexture(gl.TEXTURE_2D, gl.createTexture());
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, textureSize, textureSize, 0, gl.RGBA, gl.UNSIGNED_BYTE, textureData());
            gl.generateMipmap(gl.TEXTURE_2D);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.REPEAT);
            gl.uniform1i(gl.getUniformLocation(program, "uTexture"), 0);

            // The vertex shader makes the corners from gl_VertexID, and needs no attributes.
            gl.bindVertexArray(gl.createVertexArray());
            gl.viewport(0, 0, size, size);
            gl.enable(gl.DEPTH_TEST);
            gl.depthFunc(gl.LESS);

            for (const parity of [0, 1]) {
                draw(gl, parity, [layerCount - 1], true);
                const reference = new Uint8Array(size * size * 4);
                gl.readPixels(0, 0, size, size, gl.RGBA, gl.UNSIGNED_BYTE, reference);
                references.push(reference);
            }
        },
        iteration(gl, n) {
            const layers = [];
            for (let layer = 0; layer < layerCount; ++layer)
                layers.push(layer);
            draw(gl, n, layers, false);
        },
        probe(gl, n) {
            const y = probeRow(n);
            const pixels = new Uint8Array(stripWidth * 4);
            gl.readPixels(stripStart(y), y, stripWidth, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
            WebGLBench.verifyPixels("Pixels across the shared edge on row " + y, pixels, expectedStrip(n, y), stripWidth, tolerance);
        },
        verify(gl, n) {
            WebGLBench.verifyContextAttributes(gl, { antialias: options.multisample, depth: true });
            const image = new Uint8Array(size * size * 4);
            gl.readPixels(0, 0, size, size, gl.RGBA, gl.UNSIGNED_BYTE, image);
            // The strips of the checked rows, one row of the result per row of the image.
            const actual = new Uint8Array((size - centerRows) * stripWidth * 4);
            const expected = new Uint8Array((size - centerRows) * stripWidth * 4);
            let offset = 0;
            for (let y = 0; y < size; ++y) {
                if (isCenterRow(y))
                    continue;
                const start = (y * size + stripStart(y)) * 4;
                actual.set(image.subarray(start, start + stripWidth * 4), offset);
                expected.set(expectedStrip(n, y), offset);
                offset += stripWidth * 4;
            }
            WebGLBench.verifyPixels("Pixels across the shared edge, by checked row", actual, expected, stripWidth, tolerance);
        }
    });
};
})();
