// Each run creates a context for a new canvas, draws to the canvas and reads back one pixel, then
// drops the context and the canvas and collects garbage, so that the next run starts from scratch.
// The ContextCreate/*.html benchmarks call this with each combination of the context attributes
// alpha, antialias, stencil, depth and preserveDrawingBuffer.
function runContextCreate(attributes)
{
    const size = 2048;

    const vertexShader = `
attribute vec2 aPosition;
void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
}`;

    const fragmentShader = `
precision highp float;
uniform vec4 uColor;
void main() {
    gl_FragColor = uColor;
}`;

    const pixel = new Uint8Array(4);

    function drawWithNewContext(n)
    {
        const context = WebGLBench.createContext("webgl", size, size, attributes);
        const program = WebGLBench.createProgram(context, vertexShader, fragmentShader, ["aPosition"]);
        const color = WebGLBench.color(n, 0);
        context.uniform4f(context.getUniformLocation(program, "uColor"), color[0] / 255, color[1] / 255, color[2] / 255, 1);
        WebGLBench.bindFullscreenQuad(context);
        context.drawArrays(context.TRIANGLES, 0, 6);
        context.readPixels(size - 1, size - 1, 1, 1, context.RGBA, context.UNSIGNED_BYTE, pixel);
    }

    const attributeList = Object.entries(attributes).map(([name, value]) => name + ": " + value).join(", ");

    WebGLBench.run({
        description: "Measures creating a WebGL context with " + attributeList + " for a " + size + "x" + size + " canvas, compiling a program, drawing to the canvas and reading back 1 pixel, then dropping the context and the canvas and collecting garbage.",
        setup(gl) {
        },
        iteration(gl, n) {
            drawWithNewContext(n);
            // The context, its canvas and its objects are unreachable once drawWithNewContext() returns.
            PerfTestRunner.gc();
        },
        probe(gl, n) {
            WebGLBench.verifyPixels("Canvas pixel", pixel, WebGLBench.color(n, 0), 1, 1);
        },
        verify(gl, n) {
            WebGLBench.verifyPixels("Canvas pixel", pixel, WebGLBench.color(n, 0), 1, 1);
            WebGLBench.verifyContextAttributes(WebGLBench.createContext("webgl", size, size, attributes), attributes);
        }
    });
}
