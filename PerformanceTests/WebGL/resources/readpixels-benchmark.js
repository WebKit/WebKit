// Draws a gradient that depends on the iteration into a size x size framebuffer, then reads all
// of it back with readPixels. The readPixels is the measured operation and the sync point.

function runReadPixelsBenchmark(size)
{
    const vertexShader = `
attribute vec2 aPosition;
void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
}`;

    const fragmentShader = `
precision highp float;
uniform float uSeed;
void main() {
    vec2 p = floor(gl_FragCoord.xy);
    gl_FragColor = vec4(mod(p.x, 256.0), mod(p.y, 256.0), uSeed, 255.0) / 255.0;
}`;

    let target;
    let seedLocation;
    const pixels = new Uint8Array(size * size * 4);

    function expectedPixel(n, x, y, out)
    {
        out[0] = x & 0xff;
        out[1] = y & 0xff;
        out[2] = n & 0xff;
        out[3] = 255;
        return out;
    }

    WebGLBench.run({
        description: "Measures a draw into a " + size + "x" + size + " framebuffer followed by a readPixels of all of it.",
        setup(gl) {
            target = WebGLBench.createRenderTarget(gl, size, size);
            const program = WebGLBench.createProgram(gl, vertexShader, fragmentShader, ["aPosition"]);
            seedLocation = gl.getUniformLocation(program, "uSeed");
            WebGLBench.bindFullscreenQuad(gl);
        },
        iteration(gl, n) {
            gl.uniform1f(seedLocation, n & 0xff);
            gl.drawArrays(gl.TRIANGLES, 0, 6);
            WebGLBench.readPixels(gl, target, 0, 0, size, size, pixels);
        },
        probe(gl, n) {
            const expected = [0, 0, 0, 0];
            for (let i = 0; i < 4; ++i) {
                const x = WebGLBench.hash(n * 4 + i) % size;
                const y = WebGLBench.hash(n * 4 + i + 1) % size;
                WebGLBench.verifyPixels("Pixel (" + x + ", " + y + ")", pixels.subarray((y * size + x) * 4), expectedPixel(n, x, y, expected), 1, 0);
            }
        },
        verify(gl, n) {
            WebGLBench.verifyTarget(gl, target, "Gradient", 0, (x, y, out) => expectedPixel(n, x, y, out));
        }
    });
}
