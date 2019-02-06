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
    
    adapter = await window.webgpu.requestAdapter();
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
    context.configure({ device: defaultDevice, format:"b8g8r8a8-unorm", width: canvas.width, height: canvas.height });
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
        entryPoint: "vertex_main" 
    };

    fragmentStageDescriptor = {
        module: shaderModule,
        entryPoint: "fragment_main"
    };

    pipelineDescriptor = {
        vertexStage: vertexStageDescriptor, 
        fragmentStage: fragmentStageDescriptor,
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

    // FIXME: Default a loadOp, and storeOp in the implementation for now.
    const colorAttachmentDescriptor = {
        attachment : textureView,
        clearColor : { r: 0.35, g: 0.65, b: 0.85, a: 1.0 }
    }

    let renderPassDescriptor = {
        colorAttachments : [colorAttachmentDescriptor]
    }

    let renderPassEncoder = commandBuffer.beginRenderPass(renderPassDescriptor);
    if (!renderPassEncoder) {
        testFailed("Could not create WebGPURenderPassEncoder!");
        return;
    }

    renderPassEncoder.setPipeline(renderPipeline);

    // Note that we didn't attach any buffers - the shader is generating the vertices for us.
    renderPassEncoder.draw(3, 1, 0, 0);
    let commandBufferEnd = renderPassEncoder.endPass();
    if (!commandBufferEnd) {
        testFailed("Unable to create WebGPUCommandBuffer from WeGPURenderPassEncoder::endPass!");
        return;
    }

    const queue = defaultDevice.getQueue();
    if (!queue) {
        testFailed("Unable to create default WebGPUQueue!");
        return;
    }
    queue.submit([commandBufferEnd]);

    context.present();
}