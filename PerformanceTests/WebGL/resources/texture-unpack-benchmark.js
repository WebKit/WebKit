// Uploads RGBA8 data from a Uint8Array with texSubImage2D into 8 textures, with one of the unpack
// settings that make WebKit or ANGLE repack the data on the CPU:
//
//  - "unaligned": 1000x1000, so that rows are 4000 bytes, which is not a multiple of the 256-byte
//    bytesPerRow alignment that WebGPU requires for buffer to texture copies. UNPACK_ALIGNMENT is 1.
//  - "flipY": 1024x1024 with UNPACK_FLIP_Y_WEBGL, so the rows land in reverse order.
//  - "premultiply": 1024x1024 with UNPACK_PREMULTIPLY_ALPHA_WEBGL. The expected bytes are computed
//    here as round(c * a / 255). Implementations may round differently, for example truncate, so
//    the check allows a difference of 1.
//
// Each texture is attached to a framebuffer so that it can be read back.

function runTextureUnpackBenchmark(mode)
{
    const size = mode == "unaligned" ? 1000 : 1024;
    const textureCount = 8;
    const patternCount = 4;
    const tolerance = mode == "premultiply" ? 1 : 0;
    const targets = [];
    const patterns = [];
    // What patterns[i] reads back as after the upload.
    const expectedPatterns = [];

    const descriptions = {
        unaligned: "1000x1000 RGBA8 data (4000-byte rows) with UNPACK_ALIGNMENT 1",
        flipY: "1024x1024 RGBA8 data with UNPACK_FLIP_Y_WEBGL",
        premultiply: "1024x1024 RGBA8 data with UNPACK_PREMULTIPLY_ALPHA_WEBGL",
    };

    function patternIndex(n, texture)
    {
        return (n + texture) % patternCount;
    }

    function expectedData(pattern)
    {
        const rowSize = size * 4;
        const expected = new Uint8Array(pattern.length);
        if (mode == "flipY") {
            for (let y = 0; y < size; ++y)
                expected.set(pattern.subarray((size - 1 - y) * rowSize, (size - y) * rowSize), y * rowSize);
        } else if (mode == "premultiply") {
            for (let offset = 0; offset < pattern.length; offset += 4) {
                const alpha = pattern[offset + 3];
                for (let c = 0; c < 3; ++c)
                    expected[offset + c] = Math.round(pattern[offset + c] * alpha / 255);
                expected[offset + 3] = alpha;
            }
        } else
            expected.set(pattern);
        return expected;
    }

    WebGLBench.run({
        description: "Measures texSubImage2D of " + descriptions[mode] + " from a Uint8Array into 8 textures, followed by a 1x1 readPixels of the last texture.",
        setup(gl) {
            for (let i = 0; i < textureCount; ++i)
                targets.push(WebGLBench.createRenderTarget(gl, size, size));
            for (let i = 0; i < patternCount; ++i) {
                patterns.push(WebGLBench.createPattern(size * size * 4, i));
                expectedPatterns.push(expectedData(patterns[i]));
            }
            if (mode == "unaligned")
                gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
            else if (mode == "flipY")
                gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
            else if (mode == "premultiply")
                gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
        },
        iteration(gl, n) {
            for (let i = 0; i < textureCount; ++i) {
                gl.bindTexture(gl.TEXTURE_2D, targets[i].texture);
                gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, size, size, gl.RGBA, gl.UNSIGNED_BYTE, patterns[patternIndex(n, i)]);
            }
            gl.bindTexture(gl.TEXTURE_2D, null);
        },
        probe(gl, n) {
            const x = WebGLBench.hash(n) % size;
            const y = WebGLBench.hash(n + 1) % size;
            const offset = (y * size + x) * 4;
            WebGLBench.probe(gl, targets[textureCount - 1], x, y, expectedPatterns[patternIndex(n, textureCount - 1)].subarray(offset, offset + 4), tolerance);
        },
        verify(gl, n) {
            for (let i = 0; i < textureCount; ++i) {
                const pixels = WebGLBench.readPixels(gl, targets[i], 0, 0, size, size);
                WebGLBench.verifyPixels("Texture " + i, pixels, expectedPatterns[patternIndex(n, i)], size, tolerance);
            }
        }
    });
}
