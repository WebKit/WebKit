// Fills an OffscreenCanvas transferred from the page with solid colours, from a dedicated worker.
//
// Messages from the page:
//   { canvas, format }        Configures the canvas, then replies "configured".
//   { color, frames }         Renders that many frames of the colour, then replies "rendered".
//   { color, loop: true }     Renders frames of the colour until "stop", recording when each finishes.
//   "stop"                    Stops the loop and replies { frameTimes }, in the page's time base.

let device;
let context;
let looping = false;
const frameTimes = [];

function now()
{
    return performance.timeOrigin + performance.now();
}

// Each frame waits for the GPU, and then for a task, so that the commit to the placeholder that
// the frame schedules runs before the next frame starts.
async function renderFrame(color)
{
    const encoder = device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
        colorAttachments: [{
            view: context.getCurrentTexture().createView(),
            clearValue: color,
            loadOp: "clear",
            storeOp: "store",
        }],
    });
    pass.end();
    device.queue.submit([encoder.finish()]);
    await device.queue.onSubmittedWorkDone();
    await new Promise(resolve => setTimeout(resolve, 0));
}

onmessage = async event => {
    const message = event.data;
    try {
        if (message === "stop") {
            looping = false;
            postMessage({ frameTimes });
            return;
        }
        if (message.canvas) {
            const adapter = navigator.gpu && await navigator.gpu.requestAdapter();
            if (!adapter) {
                postMessage({ error: "no adapter" });
                return;
            }
            device = await adapter.requestDevice();
            context = message.canvas.getContext("webgpu");
            context.configure({
                device,
                format: message.format,
                alphaMode: "opaque",
                toneMapping: { mode: "extended" },
            });
            postMessage("configured");
            return;
        }
        if (message.loop) {
            looping = true;
            while (looping) {
                await renderFrame(message.color);
                frameTimes.push(now());
            }
            return;
        }
        for (let i = 0; i < message.frames; ++i)
            await renderFrame(message.color);
        postMessage("rendered");
    } catch (error) {
        postMessage({ error: String(error) });
    }
};
