// Defines the tests of copyTo() and drawing of the VideoFrame of a WebGPU canvas filled with one color.

const webgpuCanvasFormats = ["bgra8unorm", "rgba8unorm", "rgba16float"];
const webgpuCanvasAlphaModes = ["opaque", "premultiplied"];
const webgpuCanvasColorSpaces = ["srgb", "display-p3", "srgb-linear", "display-p3-linear"];

function createTestCanvas(kind)
{
    if (kind != "canvas element")
        return new OffscreenCanvas(4, 4);
    const canvas = document.createElement("canvas");
    canvas.width = 4;
    canvas.height = 4;
    return canvas;
}

function defineWebGPUCanvasVideoFrameTests(canvasKinds)
{
    let device;
    promise_setup(async () => {
        const adapter = await navigator.gpu.requestAdapter();
        device = await adapter.requestDevice();
    });

    for (const kind of canvasKinds) {
        for (const format of webgpuCanvasFormats) {
            for (const alphaMode of webgpuCanvasAlphaModes) {
                for (const colorSpace of webgpuCanvasColorSpaces) {
                    for (const content of canvasFrameContents) {
                        promise_test(async () => {
                            const canvas = createTestCanvas(kind);
                            const context = canvas.getContext("webgpu");
                            context.configure({ device, format, alphaMode, colorSpace, toneMapping: { mode: "standard" } });
                            // The colors of a WebGPU canvas are premultiplied, and those of an opaque one are drawn as they are.
                            const [r, g, b, a] = canvasFillColor(content, true);
                            const encoder = device.createCommandEncoder();
                            const pass = encoder.beginRenderPass({
                                colorAttachments: [{ view: context.getCurrentTexture().createView(), clearValue: { r, g, b, a }, loadOp: "clear", storeOp: "store" }],
                            });
                            pass.end();
                            device.queue.submit([encoder.finish()]);

                            const frame = new VideoFrame(canvas, { timestamp: 0 });
                            try {
                                const mismatches = await canvasFrameMismatches(frame, { colorSpace, isOpaque: alphaMode == "opaque", isPremultiplied: true }, content);
                                assert_equals(mismatches.join("; "), "");
                            } finally {
                                frame.close();
                                context.unconfigure();
                            }
                        }, `${kind}, ${format}, alphaMode ${alphaMode}, colorSpace ${colorSpace}, ${content.name} contents`);
                    }
                }
            }
        }
    }
}
