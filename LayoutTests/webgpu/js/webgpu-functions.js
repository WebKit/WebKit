async function getBasicDevice() {
    const adapter = await window.webgpu.requestAdapter({ powerPreference: "low-power" });
    const device = adapter.createDevice();
    return device;
}

function createBasicContext(canvas, device) {
    const context = canvas.getContext("webgpu");
    // FIXME: Implement and specify a WebGPUTextureUsageEnum.
    context.configure({ device: device, format:"b8g8r8a8-unorm", width: canvas.width, height: canvas.height });
    return context;
}

function createBasicPipeline(shaderModule, device, pipelineLayout, inputStateDescriptor, primitiveTopology = "triangleStrip") {
    const vertexStageDescriptor = {
        module: shaderModule,
        entryPoint: "vertex_main" 
    };

    const fragmentStageDescriptor = {
        module: shaderModule,
        entryPoint: "fragment_main"
    };

    const pipelineDescriptor = {
        vertexStage: vertexStageDescriptor,
        fragmentStage: fragmentStageDescriptor,
        primitiveTopology: primitiveTopology
    };

    if (pipelineLayout)
        pipelineDescriptor.layout = pipelineLayout;

    if (inputStateDescriptor)
        pipelineDescriptor.inputState = inputStateDescriptor;

    return device.createRenderPipeline(pipelineDescriptor);
}

function beginBasicRenderPass(context, commandBuffer) {
    const basicAttachment = {
        attachment: context.getNextTexture().createDefaultTextureView(),
        clearColor: { r: 1.0, g: 0, b: 0, a: 1.0 }
    }

    // FIXME: Flesh out the rest of WebGPURenderPassDescriptor. 
    return commandBuffer.beginRenderPass({ colorAttachments : [basicAttachment] });
}

function encodeBasicCommands(renderPassEncoder, renderPipeline, vertexBuffer) {
    if (vertexBuffer)
        renderPassEncoder.setVertexBuffers(0, [vertexBuffer], [0]);
    renderPassEncoder.setPipeline(renderPipeline);
    renderPassEncoder.draw(4, 1, 0, 0);
    return renderPassEncoder.endPass();
}