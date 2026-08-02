if (window.testRunner)
    testRunner.waitUntilDone();

const computeShaderSource = `@compute @workgroup_size(1)
fn computeMain() {
}`;

const vertexShaderSource = `@vertex
fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4<f32> {
    let positions = array(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(3.0, -1.0),
        vec2<f32>(-1.0, 3.0)
    );
    return vec4<f32>(positions[vertexIndex], 0.0, 1.0);
}`;

const fragmentShaderSource = `@fragment
fn fragmentMain() -> @location(0) vec4<f32> {
    return vec4<f32>(1.0, 0.0, 0.0, 1.0);
}`;

let adapter = null;
let device = null;
let presentationFormat = null;
let computePipelines = [];
let renderPipelines = [];
let offscreenWebGPUCanvas = null;
let offscreenWebGPUCanvasContext = null;
let garbageCollectionInterval = null;

async function initializeWebGPU() {
    adapter = await navigator.gpu.requestAdapter();
    device = await adapter.requestDevice();
    presentationFormat = navigator.gpu.getPreferredCanvasFormat();

    await createComputePipeline({label: "Labeled Compute Pipeline"});
    await createRenderPipeline({label: "Labeled Render Pipeline"});
}

async function createComputePipeline({asynchronously = false, label = ""} = {}) {
    let shaderModule = device.createShaderModule({code: computeShaderSource});
    let descriptor = {
        label,
        layout: "auto",
        compute: {
            module: shaderModule,
            entryPoint: "computeMain",
        },
    };
    let pipeline = asynchronously ? await device.createComputePipelineAsync(descriptor) : device.createComputePipeline(descriptor);
    computePipelines.push(pipeline);
}

async function createRenderPipeline({asynchronously = false, label = ""} = {}) {
    let vertexShaderModule = device.createShaderModule({code: vertexShaderSource});
    let fragmentShaderModule = device.createShaderModule({code: fragmentShaderSource});
    let descriptor = {
        label,
        layout: "auto",
        vertex: {
            module: vertexShaderModule,
            entryPoint: "vertexMain",
        },
        fragment: {
            module: fragmentShaderModule,
            entryPoint: "fragmentMain",
            targets: [{format: presentationFormat}],
        },
        primitive: {topology: "triangle-list"},
    };
    let pipeline = asynchronously ? await device.createRenderPipelineAsync(descriptor) : device.createRenderPipeline(descriptor);
    renderPipelines.push(pipeline);
}

function createFragmentlessRenderPipeline() {
    let shaderModule = device.createShaderModule({code: vertexShaderSource});
    let pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: {
            module: shaderModule,
            entryPoint: "vertexMain",
        },
        primitive: {topology: "triangle-list"},
    });
    renderPipelines.push(pipeline);
}

function renderToContext(context) {
    let commandEncoder = device.createCommandEncoder();
    let renderPassEncoder = commandEncoder.beginRenderPass({
        colorAttachments: [{
            view: context.getCurrentTexture().createView(),
            clearValue: {r: 0, g: 0, b: 0, a: 1},
            loadOp: "clear",
            storeOp: "store",
        }],
    });
    renderPassEncoder.setPipeline(renderPipelines[0]);
    renderPassEncoder.draw(3);
    renderPassEncoder.end();
    device.queue.submit([commandEncoder.finish()]);
}

async function renderAndReadPixel(encodeRenderPass) {
    const bytesPerRow = 256;
    let texture = device.createTexture({
        size: [1, 1],
        format: presentationFormat,
        usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });
    let readbackBuffer = device.createBuffer({
        size: bytesPerRow,
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
    });

    let commandEncoder = device.createCommandEncoder();
    let renderPassEncoder = commandEncoder.beginRenderPass({
        colorAttachments: [{
            view: texture.createView(),
            clearValue: {r: 0, g: 0, b: 0, a: 1},
            loadOp: "clear",
            storeOp: "store",
        }],
    });
    encodeRenderPass(renderPassEncoder);
    renderPassEncoder.end();
    commandEncoder.copyTextureToBuffer({texture}, {buffer: readbackBuffer, bytesPerRow}, [1, 1]);
    device.queue.submit([commandEncoder.finish()]);

    await readbackBuffer.mapAsync(GPUMapMode.READ);
    let pixel = new Uint8Array(readbackBuffer.getMappedRange()).slice(0, 4).join(",");
    readbackBuffer.unmap();
    readbackBuffer.destroy();
    texture.destroy();
    return pixel;
}

async function renderWithPipeline(eventName) {
    let content = await renderAndReadPixel((renderPassEncoder) => {
        renderPassEncoder.setPipeline(renderPipelines[0]);
        renderPassEncoder.draw(3);
    });
    TestPage.dispatchEventToFrontend(eventName, {content});
}

async function renderWithRenderBundle(eventName) {
    let renderBundleEncoder = device.createRenderBundleEncoder({colorFormats: [presentationFormat]});
    renderBundleEncoder.setPipeline(renderPipelines[0]);
    renderBundleEncoder.draw(3);
    let renderBundle = renderBundleEncoder.finish();

    let content = await renderAndReadPixel((renderPassEncoder) => {
        renderPassEncoder.executeBundles([renderBundle]);
    });
    TestPage.dispatchEventToFrontend(eventName, {content});
}

async function renderWithOffscreenPipeline(eventName) {
    if (!offscreenWebGPUCanvasContext) {
        offscreenWebGPUCanvas = new OffscreenCanvas(4, 4);
        offscreenWebGPUCanvasContext = offscreenWebGPUCanvas.getContext("webgpu");
        offscreenWebGPUCanvasContext.configure({
            device,
            format: presentationFormat,
            alphaMode: "opaque",
        });
    }

    renderToContext(offscreenWebGPUCanvasContext);
    await device.queue.onSubmittedWorkDone();
    TestPage.dispatchEventToFrontend(eventName, {rendered: true});
}

function startCollectingGarbage() {
    if (!garbageCollectionInterval)
        garbageCollectionInterval = setInterval(() => { GCController.collect(); }, 0);
}

function stopCollectingGarbage() {
    clearInterval(garbageCollectionInterval);
    garbageCollectionInterval = null;
}

function releaseLastComputePipeline() {
    computePipelines.pop();
    startCollectingGarbage();
}

function releaseLastRenderPipeline() {
    renderPipelines.pop();
    startCollectingGarbage();
}

function releaseDeviceKeepingPipelines() {
    if (offscreenWebGPUCanvasContext)
        offscreenWebGPUCanvasContext.unconfigure();
    offscreenWebGPUCanvasContext = null;
    offscreenWebGPUCanvas = null;

    device = null;
    adapter = null;
    startCollectingGarbage();
}

TestPage.registerInitializer(() => {
    InspectorTest.WebGPU = {};

    InspectorTest.WebGPU.canvas = function() {
        return Array.from(WI.canvasManager.canvasCollection).find((canvas) => canvas.contextType === WI.Canvas.ContextType.WebGPU);
    };

    InspectorTest.WebGPU.shaderProgramForType = function(canvas, programType) {
        return Array.from(canvas.shaderProgramCollection).find((shaderProgram) => shaderProgram.programType === programType);
    };
});
