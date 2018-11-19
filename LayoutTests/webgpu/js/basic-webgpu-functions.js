let shaderCode = `
#include <metal_stdlib>
    
using namespace metal;

struct Vertex
{
    float4 position [[position]];
};

vertex Vertex vertex_main(uint vid [[vertex_id]])
{
    Vertex v;
    switch (vid) {
    case 0:
        v.position = float4(-.75, -.75, 0, 1);
        break;
    case 1:
        v.position = float4(.75, -.75, 0, 1);
        break;
    case 2:
        v.position = float4(0, .75, 0, 1);
        break;
    default:
        v.position = float4(0, 0, 0, 1);
    }
    return v;
}

fragment float4 fragment_main(Vertex vertexIn [[stage_in]])
{
    return float4(1.0, 0.0, 0.0, 1.0);
}
`

let context, adapter, defaultDevice;

function runWebGPUTests(tests) {
    runWebGPUTestsOnCanvas(document.createElement("canvas"), tests);
}

async function runWebGPUTestsOnCanvas(canvas, tests) {
    try {
        await setUpContexts(canvas);

        for (let test of tests)
            test();

        debug("All tests complete.");
    } catch (error) {
        console.error(error);
        testFailed(`[${error}]: See console!`);
    }
}

async function setUpContexts(canvas) {
    context = canvas.getContext("webgpu");
    if (!context)
        testFailed("Could not create WebGPU context!");

    shouldBeDefined(window.webgpu);
    
    // FIXME: requestAdapter should take a WebGPUAdapterDescriptor.
    adapter = await window.webgpu.requestAdapter({});
    if (!adapter) {
        testFailed("Could not create default WebGPUAdapter!")
        return;
    }

    // FIXME: requestDevice should take a WebGPUDeviceDescriptor.
    defaultDevice = adapter.createDevice();
    if (!defaultDevice) {
        testFailed("Could not create WebGPUDevice!");
        return;
    }

    // FIXME: Implement WebGPUTextureUsageEnum.
    context.configure({ device: defaultDevice, format:"B8G8R8A8Unorm", width: canvas.width, height: canvas.height });
}

let shaderModule, vertexStageDescriptor, fragmentStageDescriptor, pipelineDescriptor, renderPipeline;

function setUpModule() {
    shaderModule = defaultDevice.createShaderModule({ code: shaderCode });
    if (!shaderModule) {
        testFailed("Could not create WebGPUShaderModule!");
        return;
    }
}

function setUpPipelineDescriptor() {
    vertexStageDescriptor = { 
        module: shaderModule, 
        stage: WebGPUShaderStage.VERTEX, 
        entryPoint: "vertex_main" 
    };

    fragmentStageDescriptor = {
        module: shaderModule,
        stage: WebGPUShaderStage.FRAGMENT,
        entryPoint: "fragment_main"
    };

    pipelineDescriptor = {
        stages: [vertexStageDescriptor, fragmentStageDescriptor],
        primitiveTopology: "triangleList"
    };
}

function setUpPipeline() {
    setUpModule();
    setUpPipelineDescriptor();

    renderPipeline = defaultDevice.createRenderPipeline(pipelineDescriptor);
    if (!renderPipeline) {
        testFailed("Could not create WebGPURenderPipeline!");
        return;
    }
}

function render() {
    let commandBuffer = defaultDevice.createCommandBuffer();
    if (!commandBuffer) {
        testFailed("Could not create WebGPUCommandBuffer!");
        return;
    }

    let texture = context.getNextTexture();
    if (!texture) {
        testFailed("Could not get next WebGPUTexture!");
        return;
    }

    let textureView = texture.createDefaultTextureView();
    if (!textureView) {
        testFailed("Could not create WebGPUTextureView!");
        return;
    }

    // FIXME: Rest of rendering commands to follow.
}