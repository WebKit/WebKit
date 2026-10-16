// Each run uploads 64 small tiles into one 1024x1024 texture, an atlas, with texSubImage2D(), then
// samples a texel of the last tile in a draw and reads back the drawn pixel. This is how a page
// updates a glyph atlas or a sprite sheet: many small uploads into one texture that it draws with.
//
// The state says whether the GPU uses the atlas while it is uploaded to:
//
//  - "idle": nothing before the uploads. Each run starts after probe() has read back the previous
//    run, so the GPU is not using the atlas.
//  - "used": before the uploads, a draw to the 8x8 framebuffer samples a texel of the atlas. So the
//    active render pass uses the atlas that the uploads write. An implementation that writes the
//    atlas on the CPU has to wait for the GPU first, and one that writes it on the GPU has to order
//    the writes after the draw.
//  - "busy": as "used", but first busyDrawCount full-screen draws to a separate 1024x1024
//    framebuffer also sample the atlas, with a fragment shader that loops busyIterations times
//    and additive blending, so that the GPU can't skip them. So the GPU has milliseconds of work
//    queued that uses the atlas. An implementation that writes the atlas on the CPU waits for all
//    of it, and one that writes it on the GPU does not wait.
//
// The tiles of a run are the next 64 cells of a grid over the atlas, so that each run writes other
// cells than the run before it, and each tile is one color that changes with n and the tile.
// setup() fills the whole atlas once, so that no upload needs the atlas initialized first.
//
// Check: probe() reads the pixel that the last draw wrote, which is the color of the last tile.
// verify() reads back every tile of the run through a framebuffer that the atlas is attached to.
(function () {
"use strict";

const atlasSize = 1024;
const tileCount = 64;
const sampleTargetSize = 8;
const busyTargetSize = 1024;
const busyDrawCount = 4;
const busyIterations = 64;

const vertexShader = `#version 300 es
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}`;

const fragmentShader = `#version 300 es
precision highp float;
uniform highp sampler2D uAtlas;
uniform highp ivec2 uTexel;
out vec4 outColor;
void main() {
    outColor = texelFetch(uAtlas, uTexel, 0);
}`;

const busyFragmentShader = `#version 300 es
precision highp float;
uniform highp sampler2D uAtlas;
uniform int uIterations;
// Zero for all draws but the last, so that the result does not depend on the number of draws.
uniform float uScale;
out vec4 outColor;
void main() {
    float v = texelFetch(uAtlas, ivec2(gl_FragCoord.xy), 0).r;
    for (int i = 0; i < uIterations; ++i)
        v = fract(sin(v * 12.9898 + float(i)) * 43758.5453);
    outColor = vec4(v * uScale);
}`;

const formats = {
    r8: { internalFormat: "R8", format: "RED", channels: 1 },
    rgba8: { internalFormat: "RGBA8", format: "RGBA", channels: 4 },
};

// options: { format, tileSize, state }. format is "r8" or "rgba8".
window.runTextureAtlasUpload = function (options) {
    const format = formats[options.format];
    const tileSize = options.tileSize;
    const cellsPerRow = atlasSize / tileSize;
    const cellCount = cellsPerRow * cellsPerRow;
    const used = options.state == "used" || options.state == "busy";
    const busy = options.state == "busy";

    let atlas;
    let sampleTarget;
    let checkFramebuffer;
    let texelLocation;
    let program;
    let busyTarget;
    let busyProgram;
    let busyScaleLocation;
    const tileData = [];
    const color = [0, 0, 0, 0];

    // The cell of tile i of run n, as [x, y] in texels.
    function tileOrigin(n, i)
    {
        const cell = (n * tileCount + i) % cellCount;
        return [(cell % cellsPerRow) * tileSize, Math.floor(cell / cellsPerRow) * tileSize];
    }

    // The color of tile i of run n as an RGBA8 readback of the atlas returns it.
    function tileColor(n, i)
    {
        const c = WebGLBench.color(n, i);
        return format.channels == 1 ? [c[0], 0, 0, 255] : c;
    }

    function sample(gl, n, i)
    {
        const [x, y] = tileOrigin(n, i);
        gl.bindFramebuffer(gl.FRAMEBUFFER, sampleTarget.framebuffer);
        gl.viewport(0, 0, sampleTargetSize, sampleTargetSize);
        gl.uniform2i(texelLocation, x + tileSize / 2, y + tileSize / 2);
        gl.drawArrays(gl.TRIANGLES, 0, 3);
    }

    WebGLBench.run({
        description: "Measures uploading " + tileCount + " tiles of " + tileSize + "x" + tileSize + " " + format.internalFormat
            + " into a " + atlasSize + "x" + atlasSize + " texture with texSubImage2D()"
            + (busy ? ", right after " + busyDrawCount + " full-screen draws to a " + busyTargetSize + "x" + busyTargetSize + " framebuffer that sample the texture and loop "
                + busyIterations + " times per fragment, and a draw that samples the texture," : used ? ", right after a draw that samples the texture," : "")
            + " then sampling a texel of the last tile in a draw to an 8x8 framebuffer and reading back 1 pixel.",
        contextType: "webgl2",
        setup(gl) {
            sampleTarget = WebGLBench.createRenderTarget(gl, sampleTargetSize, sampleTargetSize);
            if (busy) {
                busyTarget = WebGLBench.createRenderTarget(gl, busyTargetSize, busyTargetSize);
                busyProgram = WebGLBench.createProgram(gl, vertexShader, busyFragmentShader);
                busyScaleLocation = gl.getUniformLocation(busyProgram, "uScale");
                gl.uniform1i(gl.getUniformLocation(busyProgram, "uIterations"), busyIterations);
                gl.uniform1i(gl.getUniformLocation(busyProgram, "uAtlas"), 0);
            }
            program = WebGLBench.createProgram(gl, vertexShader, fragmentShader);
            texelLocation = gl.getUniformLocation(program, "uTexel");
            gl.uniform1i(gl.getUniformLocation(program, "uAtlas"), 0);

            atlas = gl.createTexture();
            gl.activeTexture(gl.TEXTURE0);
            gl.bindTexture(gl.TEXTURE_2D, atlas);
            gl.texStorage2D(gl.TEXTURE_2D, 1, gl[format.internalFormat], atlasSize, atlasSize);
            gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, atlasSize, atlasSize, gl[format.format], gl.UNSIGNED_BYTE,
                new Uint8Array(atlasSize * atlasSize * format.channels));
            for (let i = 0; i < tileCount; ++i)
                tileData.push(new Uint8Array(tileSize * tileSize * format.channels));

            checkFramebuffer = gl.createFramebuffer();
            gl.bindFramebuffer(gl.FRAMEBUFFER, checkFramebuffer);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, atlas, 0);
            const status = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
            if (status != gl.FRAMEBUFFER_COMPLETE)
                throw new Error("Framebuffer is not complete: 0x" + status.toString(16));

            // The vertex shader makes the corners from gl_VertexID, and needs no attributes.
            gl.bindVertexArray(gl.createVertexArray());
        },
        iteration(gl, n) {
            if (busy) {
                gl.bindFramebuffer(gl.FRAMEBUFFER, busyTarget.framebuffer);
                gl.viewport(0, 0, busyTargetSize, busyTargetSize);
                gl.useProgram(busyProgram);
                gl.enable(gl.BLEND);
                gl.blendFunc(gl.ONE, gl.ONE);
                for (let i = 0; i < busyDrawCount; ++i) {
                    gl.uniform1f(busyScaleLocation, i == busyDrawCount - 1 ? 1 : 0);
                    gl.drawArrays(gl.TRIANGLES, 0, 3);
                }
                gl.disable(gl.BLEND);
                gl.useProgram(program);
            }
            if (used)
                sample(gl, n, 0);
            for (let i = 0; i < tileCount; ++i) {
                const data = tileData[i];
                WebGLBench.colorInto(n, i, color);
                if (format.channels == 1)
                    data.fill(color[0]);
                else
                    new Uint32Array(data.buffer).fill(color[0] | color[1] << 8 | color[2] << 16 | color[3] << 24);
                const [x, y] = tileOrigin(n, i);
                gl.texSubImage2D(gl.TEXTURE_2D, 0, x, y, tileSize, tileSize, gl[format.format], gl.UNSIGNED_BYTE, data);
            }
            sample(gl, n, tileCount - 1);
        },
        probe(gl, n) {
            WebGLBench.probe(gl, sampleTarget, 0, 0, tileColor(n, tileCount - 1), 0);
        },
        verify(gl, n) {
            const target = { framebuffer: checkFramebuffer };
            for (let i = 0; i < tileCount; ++i) {
                const [x, y] = tileOrigin(n, i);
                const pixels = WebGLBench.readPixels(gl, target, x, y, tileSize, tileSize);
                WebGLBench.verifyPixelsAreColor("Tile " + i, pixels, tileColor(n, i), tileSize, 0);
            }
        }
    });
};
})();
