// Measures drawing a WebGPU canvas to a 2D canvas, as Canvas/drawImageSource*WebGL.html do for a
// WebGL canvas.
//
// options.source: "canvas" for an HTMLCanvasElement, "offscreen-canvas" for an OffscreenCanvas.
// options.format: the GPUTextureFormat the context is configured with.
// options.drawSource: "canvas" draws the WebGPU canvas, "image-bitmap" an ImageBitmap of it, and
// "video-frame" a VideoFrame of it.
//
// Each iteration renders a new frame and draws it. The frame is rendered in a new task, as the
// current texture of a WebGPU canvas is replaced only after the task it was handed out in.

const drawImageSourceWebGPUSize = 2000;
const drawImageSourceWebGPUIterations = 100;

function yieldTask()
{
    return new Promise((resolve) => {
        const channel = new MessageChannel;
        channel.port1.onmessage = () => resolve();
        channel.port2.postMessage(null);
    });
}

function reportDrawImageSourceWebGPUError(message)
{
    const element = document.createElement("pre");
    element.textContent = `FAIL: ${message}`;
    document.body.appendChild(element);
}

async function runDrawImageSourceWebGPUTest(options)
{
    const size = drawImageSourceWebGPUSize;
    let source;
    if (options.source == "offscreen-canvas")
        source = new OffscreenCanvas(size, size);
    else {
        source = document.getElementById("source");
        source.width = size;
        source.height = size;
    }

    const adapter = navigator.gpu ? await navigator.gpu.requestAdapter() : null;
    if (!adapter) {
        reportDrawImageSourceWebGPUError("WebGPU is not available.");
        return;
    }
    const device = await adapter.requestDevice();
    const context = source.getContext("webgpu");
    context.configure({ device, format: options.format, alphaMode: "premultiplied" });

    const target = document.getElementById("target");
    target.width = size;
    target.height = size;
    const ctx = target.getContext("2d");
    ctx.globalCompositeOperation = "copy";

    // Green at half opacity, with a green that changes every frame.
    function render(i)
    {
        const green = i / (drawImageSourceWebGPUIterations - 1);
        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
            colorAttachments: [{
                view: context.getCurrentTexture().createView(),
                clearValue: { r: 0, g: green * 0.5, b: 0, a: 0.5 },
                loadOp: "clear",
                storeOp: "store",
            }],
        });
        pass.end();
        device.queue.submit([encoder.finish()]);
    }

    async function draw()
    {
        if (options.drawSource == "image-bitmap") {
            const imageBitmap = await createImageBitmap(source);
            ctx.drawImage(imageBitmap, 0, 0);
            imageBitmap.close();
        } else if (options.drawSource == "video-frame") {
            const frame = new VideoFrame(source, { timestamp: performance.now() });
            ctx.drawImage(frame, 0, 0);
            frame.close();
        } else
            ctx.drawImage(source, 0, 0);
    }

    let didReportError = false;
    function verify()
    {
        // The last frame is opaque green at alpha 128 once unpremultiplied.
        const pixel = ctx.getImageData(1, 1, 1, 1).data;
        const expected = [0, 255, 0, 128];
        const tolerance = 3;
        if (didReportError || pixel.every((value, i) => Math.abs(value - expected[i]) <= tolerance))
            return;
        didReportError = true;
        reportDrawImageSourceWebGPUError(`The last frame drawn is ${Array.from(pixel)}, expected ${expected}.`);
    }

    let isDone = false;
    PerfTestRunner.prepareToMeasureValuesAsync({ done: () => { isDone = true; }, unit: "ms" });
    while (!isDone) {
        const startTime = PerfTestRunner.now();
        for (let i = 0; i < drawImageSourceWebGPUIterations; ++i) {
            render(i);
            await draw();
            await yieldTask();
        }
        // Waits for the draws, as WebKit does not have backpressure for them.
        verify();
        PerfTestRunner.measureValueAsync(PerfTestRunner.now() - startTime);
        await yieldTask();
    }
    context.unconfigure();
    device.destroy();
}
