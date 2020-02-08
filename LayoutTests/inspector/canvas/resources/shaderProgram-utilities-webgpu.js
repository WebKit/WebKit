if (window.internals)
    window.internals.settings.setWebGPUEnabled(true);

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

const computePipelineSource = `
[numthreads(2, 1, 1)]
compute void computeShader(device float[] buffer : register(u0), float3 threadID : SV_DispatchThreadID) {
    buffer[uint(threadID.x)] = buffer[uint(threadID.x)] * 2.0;
}
`;

const renderPipelineSource = `
vertex float4 vertexShader(float4 position : attribute(0), float i : attribute(1)) : SV_Position {
    return position;
}

fragment float4 fragmentShader(float4 position : SV_Position) : SV_Target 0 {
    return position;
}
`;

let device = null;

async function createComputePipeline(code = computePipelineSource) {
    // Copied from webgpu/whlsl/compute.html.
    const shaderModule = device.createShaderModule({code});
    const computeStage = {module: shaderModule, entryPoint: "computeShader"};
    const bindGroupLayoutDescriptor = {bindings: [{binding: 0, visibility: 7, type: "storage-buffer"}]};
    const bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutDescriptor);
    const pipelineLayoutDescriptor = {bindGroupLayouts: [bindGroupLayout]};
    const pipelineLayout = device.createPipelineLayout(pipelineLayoutDescriptor);
    device.createComputePipeline({computeStage, layout: pipelineLayout});
}

async function createRenderPipeline(code = renderPipelineSource) {
    // Copied from webgpu/whlsl/whlsl.html.
    const shaderModule = device.createShaderModule({code});
    const vertexStage = {module: shaderModule, entryPoint: "vertexShader"};
    const fragmentShaderModule = device.createShaderModule({code});
    const fragmentStage = {module: shaderModule, entryPoint: "fragmentShader"};
    const primitiveTopology = "triangle-strip";
    const rasterizationState = {frontFace: "cw", cullMode: "none"};
    const alphaBlend = {};
    const colorBlend = {};
    const colorStates = [{format: "rgba8unorm", alphaBlend, colorBlend, writeMask: 15}]; // GPUColorWrite.ALL
    const depthStencilState = null;
    const attribute0 = {shaderLocation: 0, format: "float4"};
    const attribute1 = {shaderLocation: 1, format: "float"};
    const input0 = {stride: 16, attributeSet: [attribute0]};
    const input1 = {stride: 4, attributeSet: [attribute1]};
    const inputs = [input0, input1];
    const vertexInput = {vertexBuffers: inputs};
    const bindGroupLayoutDescriptor = {bindings: [{binding: 0, visibility: 7, type: "uniform-buffer"}]};
    const bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutDescriptor);
    const pipelineLayoutDescriptor = {bindGroupLayouts: [bindGroupLayout]};
    const pipelineLayout = device.createPipelineLayout(pipelineLayoutDescriptor);
    device.createRenderPipeline({vertexStage, fragmentStage, primitiveTopology, rasterizationState, colorStates, depthStencilState, vertexInput, sampleCount: 1, layout: pipelineLayout});
}

function deleteDevice() {
    device = null;
    // Force GC to make sure the device is destroyed, otherwise the frontend
    // does not receive WI.CanvasManager.Event.CanvasRemoved events.
    setTimeout(() => { GCController.collect(); }, 0);
}

async function load() {
    let adapter = await navigator.gpu.requestAdapter();
    device = await adapter.requestDevice();

    await Promise.all([
        createComputePipeline(),
        createRenderPipeline(),
    ]);

    if (window.beforeTest)
        await beforeTest();

    runTest();
}

TestPage.registerInitializer(() => {
    if (!InspectorTest.ShaderProgram)
        InspectorTest.ShaderProgram = {};

    InspectorTest.ShaderProgram.programForType = function(programType) {
        let result = null;
        for (let shaderProgram of WI.canvasManager.shaderPrograms) {
            if (shaderProgram.programType === programType) {
                InspectorTest.assert(!result);
                result = shaderProgram;
            }
        }
        return result;
    };
});
