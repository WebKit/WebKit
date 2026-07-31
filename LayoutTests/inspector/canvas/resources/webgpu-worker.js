let adapter = null;
let device = null;
let offscreenCanvas = null;
let offscreenContext = null;

async function initialize() {
    adapter = await navigator.gpu.requestAdapter();
    device = await adapter.requestDevice();

    let format = navigator.gpu.getPreferredCanvasFormat();
    offscreenCanvas = new OffscreenCanvas(4, 4);
    offscreenContext = offscreenCanvas.getContext("webgpu");
    offscreenContext.configure({device, format});

    postMessage("ready");
}

initialize();
