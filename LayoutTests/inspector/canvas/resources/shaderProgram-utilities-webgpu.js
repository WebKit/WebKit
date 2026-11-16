if (window.testRunner)
    testRunner.waitUntilDone();

const computeShaderSource = `@compute @workgroup_size(1)
fn computeMain() {
}`;

const updatedComputeShaderSource = `// Updated compute shader.
@compute @workgroup_size(1)
fn computeMain() {
}`;

const boundComputeShaderSource = `@group(0) @binding(0) var<storage, read_write> result: array<u32>;

@compute @workgroup_size(1)
fn computeMain() {
    result[0] = 1;
}`;

const updatedBoundComputeShaderSource = `// Updated bound compute shader.
${boundComputeShaderSource}`;

const invalidShaderSource = `This is not valid WGSL.`;

const vertexShaderSource = `@vertex
fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4<f32> {
    let positions = array(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(3.0, -1.0),
        vec2<f32>(-1.0, 3.0)
    );
    return vec4<f32>(positions[vertexIndex], 0.0, 1.0);
}`;

const updatedVertexShaderSource = `// Updated vertex shader.
@vertex
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

const updatedFragmentShaderSource = `// Updated fragment shader.
@fragment
fn fragmentMain() -> @location(0) vec4<f32> {
    return vec4<f32>(0.0, 1.0, 0.0, 1.0);
}`;

const sharedRenderShaderSource = `${vertexShaderSource}

${fragmentShaderSource}`;

const updatedSharedRenderShaderSource = `// Updated shared shader module.
${vertexShaderSource}

${updatedFragmentShaderSource}`;

const updatedAgainSharedRenderShaderSource = `// Updated shared shader module again.
${updatedVertexShaderSource}

${fragmentShaderSource}`;

const boundRenderShaderSource = `struct Uniforms {
    color: vec4<f32>,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

${vertexShaderSource}

@fragment
fn fragmentMain() -> @location(0) vec4<f32> {
    return uniforms.color;
}`;

const updatedBoundRenderShaderSource = `// Updated bound render shader.
${boundRenderShaderSource}`;

const updatedAgainBoundRenderShaderSource = `// Updated bound render shader again.
${boundRenderShaderSource}`;

let adapter = null;
let device = null;
let presentationFormat = null;
let computePipelines = [];
let renderPipelines = [];
let boundComputePipelineState = null;
let boundRenderPipelineStates = [];
let reusableSharedRenderShaderModule = null;
let webGPUCanvasContext = null;
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

function createBoundComputePipeline() {
    let shaderModule = device.createShaderModule({code: boundComputeShaderSource});
    let pipeline = device.createComputePipeline({
        layout: "auto",
        compute: {
            module: shaderModule,
            entryPoint: "computeMain",
        },
    });
    let buffer = device.createBuffer({
        size: 4,
        usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
    });
    let bindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [{binding: 0, resource: {buffer}}],
    });

    computePipelines.push(pipeline);
    boundComputePipelineState = {pipeline, bindGroup};
}

async function dispatchBoundComputePipeline(eventName) {
    device.pushErrorScope("validation");

    let commandEncoder = device.createCommandEncoder();
    let computePassEncoder = commandEncoder.beginComputePass();
    computePassEncoder.setPipeline(boundComputePipelineState.pipeline);
    computePassEncoder.setBindGroup(0, boundComputePipelineState.bindGroup);
    computePassEncoder.dispatchWorkgroups(1);
    computePassEncoder.end();
    device.queue.submit([commandEncoder.finish()]);
    await device.queue.onSubmittedWorkDone();

    let error = await device.popErrorScope();
    TestPage.dispatchEventToFrontend(eventName, {error: error ? error.message : null});
}

function pushValidationErrorScope() {
    device.pushErrorScope("validation");
}

async function popValidationErrorScope(eventName) {
    let error = await device.popErrorScope();
    TestPage.dispatchEventToFrontend(eventName, {error: error ? error.message : null});
}

async function createRenderPipeline({asynchronously = false, label = "", sharedShaderModule = false, reuseSharedShaderModule = false} = {}) {
    let vertexShaderModule = null;
    let fragmentShaderModule = null;

    if (sharedShaderModule) {
        if (reuseSharedShaderModule && reusableSharedRenderShaderModule)
            vertexShaderModule = reusableSharedRenderShaderModule;
        else {
            vertexShaderModule = device.createShaderModule({code: sharedRenderShaderSource});
            if (reuseSharedShaderModule)
                reusableSharedRenderShaderModule = vertexShaderModule;
        }
        fragmentShaderModule = vertexShaderModule;
    } else {
        vertexShaderModule = device.createShaderModule({code: vertexShaderSource});
        fragmentShaderModule = device.createShaderModule({code: fragmentShaderSource});
    }

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

function createBoundRenderPipeline({explicitLayout = false} = {}) {
    let shaderModule = device.createShaderModule({code: boundRenderShaderSource});
    let bindGroupLayout = null;
    let layout = "auto";
    if (explicitLayout) {
        bindGroupLayout = device.createBindGroupLayout({
            entries: [{
                binding: 0,
                visibility: GPUShaderStage.FRAGMENT,
                buffer: {type: "uniform"},
            }],
        });
        layout = device.createPipelineLayout({bindGroupLayouts: [bindGroupLayout]});
    }

    let pipeline = device.createRenderPipeline({
        layout,
        vertex: {
            module: shaderModule,
            entryPoint: "vertexMain",
        },
        fragment: {
            module: shaderModule,
            entryPoint: "fragmentMain",
            targets: [{format: presentationFormat}],
        },
        primitive: {topology: "triangle-list"},
    });
    let buffer = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    device.queue.writeBuffer(buffer, 0, new Float32Array([1, 0, 0, 1]));
    let bindGroup = device.createBindGroup({
        layout: bindGroupLayout || pipeline.getBindGroupLayout(0),
        entries: [{binding: 0, resource: {buffer}}],
    });

    renderPipelines.push(pipeline);
    boundRenderPipelineStates.push({pipeline, bindGroup});
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

async function renderAndReadPixel(texture, encodeRenderPass) {
    const bytesPerRow = 256;
    let shouldDestroyTexture = !texture;
    if (!texture) {
        texture = device.createTexture({
            size: [1, 1],
            format: presentationFormat,
            usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
        });
    }
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
    if (shouldDestroyTexture)
        texture.destroy();
    return pixel;
}

async function renderWithPipeline(eventName) {
    if (!webGPUCanvasContext) {
        webGPUCanvasContext = document.getElementById("webgpu-canvas").getContext("webgpu");
        webGPUCanvasContext.configure({
            device,
            format: presentationFormat,
            usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
            alphaMode: "opaque",
        });
    }

    let content = await renderAndReadPixel(webGPUCanvasContext.getCurrentTexture(), (renderPassEncoder) => {
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

    let content = await renderAndReadPixel(null, (renderPassEncoder) => {
        renderPassEncoder.executeBundles([renderBundle]);
    });
    TestPage.dispatchEventToFrontend(eventName, {content});
}

async function renderWithBoundPipeline(eventName, index) {
    device.pushErrorScope("validation");

    let state = boundRenderPipelineStates[index];
    await renderAndReadPixel(null, (renderPassEncoder) => {
        renderPassEncoder.setPipeline(state.pipeline);
        renderPassEncoder.setBindGroup(0, state.bindGroup);
        renderPassEncoder.draw(3);
    });

    let error = await device.popErrorScope();
    TestPage.dispatchEventToFrontend(eventName, {error: error ? error.message : null});
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
    if (webGPUCanvasContext)
        webGPUCanvasContext.unconfigure();
    webGPUCanvasContext = null;

    if (offscreenWebGPUCanvasContext)
        offscreenWebGPUCanvasContext.unconfigure();
    offscreenWebGPUCanvasContext = null;
    offscreenWebGPUCanvas = null;

    reusableSharedRenderShaderModule = null;
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
