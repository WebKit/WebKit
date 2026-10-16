// Uploads size x size RGBA8 data from a Uint8Array into 8 textures, with either texSubImage2D
// into the existing storage or texImage2D, which redefines the texture on every upload. Each
// texture is attached to a framebuffer so that it can be read back.

function runTextureUploadBenchmark(uploadFunction, size)
{
    const textureCount = 8;
    const patternCount = 4;
    const targets = [];
    const patterns = [];

    function patternFor(n, texture)
    {
        return patterns[(n + texture) % patternCount];
    }

    WebGLBench.run({
        description: "Measures " + uploadFunction + " of " + size + "x" + size + " RGBA8 data from a Uint8Array into 8 textures, followed by a 1x1 readPixels of the last texture.",
        setup(gl) {
            for (let i = 0; i < textureCount; ++i)
                targets.push(WebGLBench.createRenderTarget(gl, size, size));
            for (let i = 0; i < patternCount; ++i)
                patterns.push(WebGLBench.createPattern(size * size * 4, i));
        },
        iteration(gl, n) {
            for (let i = 0; i < textureCount; ++i) {
                gl.bindTexture(gl.TEXTURE_2D, targets[i].texture);
                if (uploadFunction == "texImage2D")
                    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, size, size, 0, gl.RGBA, gl.UNSIGNED_BYTE, patternFor(n, i));
                else
                    gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, size, size, gl.RGBA, gl.UNSIGNED_BYTE, patternFor(n, i));
            }
            gl.bindTexture(gl.TEXTURE_2D, null);
        },
        probe(gl, n) {
            const x = WebGLBench.hash(n) % size;
            const y = WebGLBench.hash(n + 1) % size;
            const offset = (y * size + x) * 4;
            WebGLBench.probe(gl, targets[textureCount - 1], x, y, patternFor(n, textureCount - 1).subarray(offset, offset + 4));
        },
        verify(gl, n) {
            for (let i = 0; i < textureCount; ++i) {
                const pixels = WebGLBench.readPixels(gl, targets[i], 0, 0, size, size);
                WebGLBench.verifyPixels("Texture " + i, pixels, patternFor(n, i), size, 0);
            }
        }
    });
}
