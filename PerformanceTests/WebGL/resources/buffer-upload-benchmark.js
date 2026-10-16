// Uploads chunkCount chunks of chunkSize bytes with bufferSubData into one vertex buffer, then
// draws one point per pixel of a framebuffer with a single drawArrays that uses all of the
// buffer. The buffer holds the RGBA color of each point as normalized bytes, so every uploaded
// byte ends up in exactly one pixel and the rendered image proves that every upload landed.

function runBufferUploadBenchmark(chunkSize, chunkCount)
{
    const bufferSize = chunkSize * chunkCount;
    const vertexCount = bufferSize / 4;
    const width = vertexCount >= 1024 * 1024 ? 2048 : 512;
    const height = vertexCount / width;
    // One pattern more than chunks, so that the chunks of a run differ from each other and from
    // the chunks at the same offsets in the previous run.
    const patternCount = chunkCount + 1;

    const vertexShader = `
attribute vec2 aPosition;
attribute vec4 aColor;
varying vec4 vColor;
void main() {
    vColor = aColor;
    gl_PointSize = 1.0;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}`;

    const fragmentShader = `
precision highp float;
varying vec4 vColor;
void main() {
    gl_FragColor = vColor;
}`;

    let target;
    const patterns = [];

    function patternFor(n, chunk)
    {
        return patterns[(n + chunk) % patternCount];
    }

    function expectedPixel(n, x, y, out)
    {
        const byteOffset = (y * width + x) * 4;
        const pattern = patternFor(n, Math.floor(byteOffset / chunkSize));
        const offset = byteOffset % chunkSize;
        for (let c = 0; c < 4; ++c)
            out[c] = pattern[offset + c];
        return out;
    }

    function sizeToString(size)
    {
        return size >= 1024 * 1024 ? (size / (1024 * 1024)) + " MB" : (size / 1024) + " KB";
    }

    WebGLBench.run({
        description: "Measures " + chunkCount + " bufferSubData calls of " + sizeToString(chunkSize) + " each into a vertex buffer of " + sizeToString(bufferSize) + " and one drawArrays of " + vertexCount + " points that uses all of it, followed by a 1x1 readPixels.",
        setup(gl) {
            target = WebGLBench.createRenderTarget(gl, width, height);
            WebGLBench.createProgram(gl, vertexShader, fragmentShader, ["aPosition", "aColor"]);
            for (let i = 0; i < patternCount; ++i)
                patterns.push(WebGLBench.createPattern(chunkSize, i));

            // Vertex i is a point at the center of pixel (i % width, i / width).
            const positions = new Float32Array(vertexCount * 2);
            for (let y = 0; y < height; ++y) {
                for (let x = 0; x < width; ++x) {
                    const offset = (y * width + x) * 2;
                    positions[offset] = (x + 0.5) / (width / 2) - 1;
                    positions[offset + 1] = (y + 0.5) / (height / 2) - 1;
                }
            }
            gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
            gl.bufferData(gl.ARRAY_BUFFER, positions, gl.STATIC_DRAW);
            gl.enableVertexAttribArray(0);
            gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);

            // The color buffer stays bound to ARRAY_BUFFER for the uploads.
            gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
            gl.bufferData(gl.ARRAY_BUFFER, bufferSize, gl.DYNAMIC_DRAW);
            gl.enableVertexAttribArray(1);
            gl.vertexAttribPointer(1, 4, gl.UNSIGNED_BYTE, true, 0, 0);
        },
        iteration(gl, n) {
            for (let i = 0; i < chunkCount; ++i)
                gl.bufferSubData(gl.ARRAY_BUFFER, i * chunkSize, patternFor(n, i));
            gl.drawArrays(gl.POINTS, 0, vertexCount);
        },
        probe(gl, n) {
            const vertex = WebGLBench.hash(n) % vertexCount;
            const x = vertex % width;
            const y = Math.floor(vertex / width);
            WebGLBench.probe(gl, target, x, y, expectedPixel(n, x, y, [0, 0, 0, 0]));
        },
        verify(gl, n) {
            WebGLBench.verifyTarget(gl, target, "Points", 0, (x, y, out) => expectedPixel(n, x, y, out));
        }
    });
}
