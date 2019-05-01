async function getBasicDevice() {
    const adapter = await navigator.gpu.requestAdapter({ powerPreference: "low-power" });
    const device = await adapter.requestDevice();
    return device;
}

function createBasicSwapChain(canvas, device) {
    const context = canvas.getContext("gpu");
    return context.configureSwapChain({ device: device, format: "bgra8unorm" });
}

function createBasicDepthStateDescriptor() {
    return {
        depthWriteEnabled: true,
        depthCompare: "less"
    };
}

function createBasicDepthTexture(canvas, device) {
    const depthSize = {
        width: canvas.width,
        height: canvas.height,
        depth: 1
    };

    return device.createTexture({
        size: depthSize,
        format: "depth32float-stencil8",
        usage: GPUTextureUsage.OUTPUT_ATTACHMENT
    });
}

function createBasicPipeline(shaderModule, device, colorStates, pipelineLayout, inputStateDescriptor, depthStateDescriptor, primitiveTopology = "triangle-strip") {
    const vertexStageDescriptor = {
        module: shaderModule,
        entryPoint: "vertex_main" 
    };

    const fragmentStageDescriptor = {
        module: shaderModule,
        entryPoint: "fragment_main"
    };

    if (!colorStates) {
        colorStates = [{ 
            format: "bgra8unorm",
            alphaBlend: {},
            colorBlend: {}
        }];
    }

    if (!inputStateDescriptor)
        inputStateDescriptor = { attributes: [], inputs: [] };

    const pipelineDescriptor = {
        vertexStage: vertexStageDescriptor,
        fragmentStage: fragmentStageDescriptor,
        primitiveTopology: primitiveTopology,
        colorStates: colorStates,
        inputState: inputStateDescriptor
    };

    if (pipelineLayout)
        pipelineDescriptor.layout = pipelineLayout;

    if (depthStateDescriptor)
        pipelineDescriptor.depthStencilState = depthStateDescriptor;

    return device.createRenderPipeline(pipelineDescriptor);
}

function beginBasicRenderPass(swapChain, commandEncoder) {
    const basicAttachment = {
        attachment: swapChain.getCurrentTexture().createDefaultView(),
        loadOp: "clear",
        storeOp: "store",
        clearColor: { r: 1.0, g: 0, b: 0, a: 1.0 }
    };

    // FIXME: Flesh out the rest of GPURenderPassDescriptor. 
    return commandEncoder.beginRenderPass({ colorAttachments : [basicAttachment] });
}

function encodeBasicCommands(renderPassEncoder, renderPipeline, vertexBuffer) {
    if (vertexBuffer)
        renderPassEncoder.setVertexBuffers(0, [vertexBuffer], [0]);
    renderPassEncoder.setPipeline(renderPipeline);
    renderPassEncoder.draw(4, 1, 0, 0);
    renderPassEncoder.endPass();
}