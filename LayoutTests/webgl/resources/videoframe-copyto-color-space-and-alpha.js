// Tests copyTo() and drawing of the VideoFrame of a WebGL canvas filled with one color, with the
// expectations of canvas-videoframe-copyto-utilities.js.
"use strict";

function createVideoFrameTestCanvas(kind)
{
    if (kind != "canvas element")
        return new OffscreenCanvas(4, 4);
    const canvas = document.createElement("canvas");
    canvas.width = 4;
    canvas.height = 4;
    document.body.appendChild(canvas);
    return canvas;
}

async function testWebGLCanvasVideoFrame(kind, version, alpha, premultipliedAlpha, colorSpace, content)
{
    const name = `${kind}, ${version == 2 ? "webgl2" : "webgl"}, alpha ${alpha}, premultipliedAlpha ${premultipliedAlpha}, drawingBufferColorSpace ${colorSpace}, ${content.name} contents`;
    let canvas = createVideoFrameTestCanvas(kind);
    let gl = WebGLTestUtils.create3DContext(canvas, { alpha, premultipliedAlpha }, version);
    if (!gl) {
        testFailed(`${name}: could not create the context`);
        return { canvas };
    }
    gl.drawingBufferColorSpace = colorSpace;
    const [r, g, b, a] = canvasFillColor(content, premultipliedAlpha);
    gl.clearColor(r, g, b, a);
    gl.clear(gl.COLOR_BUFFER_BIT);
    if (gl.getError() != gl.NO_ERROR)
        testFailed(`${name}: clearing the canvas failed`);

    const frame = new VideoFrame(canvas, { timestamp: 0 });
    // The alpha of a canvas without alpha is ignored, so its colors are what it stores.
    const mismatches = await canvasFrameMismatches(frame, { colorSpace, isOpaque: !alpha, isPremultiplied: premultipliedAlpha }, content);
    frame.close();
    if (mismatches.length)
        testFailed(`${name}: ${mismatches.join("; ")}`);
    else
        testPassed(name);
    return { canvas, gl };
}

async function testWebGLCanvasVideoFrames(canvasKinds)
{
    for (const kind of canvasKinds) {
        for (const version of [1, 2]) {
            for (const alpha of [true, false]) {
                for (const premultipliedAlpha of [true, false]) {
                    for (const colorSpace of ["srgb", "display-p3"]) {
                        for (const content of canvasFrameContents) {
                            let { canvas, gl } = await testWebGLCanvasVideoFrame(kind, version, alpha, premultipliedAlpha, colorSpace, content);
                            if (canvas.remove)
                                canvas.remove();
                            canvas = null;
                            gl = null;
                            if (self.gc)
                                self.gc();
                        }
                    }
                }
            }
        }
    }
}
