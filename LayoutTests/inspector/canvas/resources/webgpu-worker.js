const computeShaderSource = `@compute @workgroup_size(1)
fn computeMain() {
}`;

const renderShaderSource = `@vertex
fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4<f32> {
    let positions = array(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(3.0, -1.0),
        vec2<f32>(-1.0, 3.0)
    );
    return vec4<f32>(positions[vertexIndex], 0.0, 1.0);
}

@fragment
fn fragmentMain() -> @location(0) vec4<f32> {
    return vec4<f32>(1.0, 0.0, 0.0, 1.0);
}`;

let adapter = null;
let device = null;
let computePipeline = null;
let renderPipeline = null;
let offscreenCanvas = null;
let offscreenContext = null;

async function initialize() {
    adapter = await navigator.gpu.requestAdapter();
    device = await adapter.requestDevice({label: "Labeled Worker WebGPU Device"});

    let computeShaderModule = device.createShaderModule({code: computeShaderSource});
    computePipeline = device.createComputePipeline({
        label: "Labeled Worker Compute Pipeline",
        layout: "auto",
        compute: {
            module: computeShaderModule,
            entryPoint: "computeMain",
        },
    });

    let renderShaderModule = device.createShaderModule({code: renderShaderSource});
    let format = navigator.gpu.getPreferredCanvasFormat();
    renderPipeline = device.createRenderPipeline({
        label: "Labeled Worker Render Pipeline",
        layout: "auto",
        vertex: {
            module: renderShaderModule,
            entryPoint: "vertexMain",
        },
        fragment: {
            module: renderShaderModule,
            entryPoint: "fragmentMain",
            targets: [{format}],
        },
        primitive: {topology: "triangle-list"},
    });

    offscreenCanvas = new OffscreenCanvas(4, 4);
    offscreenContext = offscreenCanvas.getContext("webgpu");
    offscreenContext.configure({device, format});

    let commandEncoder = device.createCommandEncoder();
    let renderPassEncoder = commandEncoder.beginRenderPass({
        colorAttachments: [{
            view: offscreenContext.getCurrentTexture().createView(),
            clearValue: {r: 0, g: 0, b: 0, a: 1},
            loadOp: "clear",
            storeOp: "store",
        }],
    });
    renderPassEncoder.setPipeline(renderPipeline);
    renderPassEncoder.draw(3);
    renderPassEncoder.end();
    device.queue.submit([commandEncoder.finish()]);
    await device.queue.onSubmittedWorkDone();

    postMessage("ready");
}

initialize();
