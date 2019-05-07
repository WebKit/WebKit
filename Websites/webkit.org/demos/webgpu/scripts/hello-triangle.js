async function helloTriangle() {
    if (!navigator.gpu) {
        document.body.className = 'error';
        return;
    }

    const adapter = await navigator.gpu.requestAdapter();
    const device = await adapter.requestDevice();
    
    /*** Shader Setup ***/
    
    /* GPUShaderModule */
    const positionLocation = 0;
    const colorLocation = 1;
    
    const shaderSource = `
    #include <metal_stdlib>
  
    using namespace metal;
  
    struct Vertex {
        float4 position [[attribute(${positionLocation})]];
        float4 color [[attribute(${colorLocation})]];
    };
  
    struct FragmentData {
        float4 position [[position]];
        float4 color;
    };
  
    vertex FragmentData vertexMain(const Vertex in [[stage_in]]) 
    {
        return FragmentData { in.position, in.color };
    }
  
    fragment float4 fragmentMain(const FragmentData in [[stage_in]])
    {
        return in.color;
    }
    `;
    const shaderModule = device.createShaderModule({ code: shaderSource });
    
    /* GPUPipelineStageDescriptors */
    const vertexStageDescriptor = { module: shaderModule, entryPoint: "vertexMain" };
    const fragmentStageDescriptor = { module: shaderModule, entryPoint: "fragmentMain" };
    
    /*** Vertex Buffer Setup ***/
    
    /* Vertex Data */
    const colorOffset = 4 * 4; // 4 floats of 4 bytes each.
    const vertexStride = 8 * 4;
    const vertexDataSize = vertexStride * 3;
    
    /* GPUBufferDescriptor */
    const vertexBufferDescriptor = { 
        size: vertexDataSize,
        usage: GPUBufferUsage.MAP_WRITE | GPUBufferUsage.VERTEX
    };
    /* GPUBuffer */
    const vertexBuffer = device.createBuffer(vertexBufferDescriptor);
    
    /*** Write Data To GPU ***/
    
    const vertexArrayBuffer = await vertexBuffer.mapWriteAsync();
    const vertexWriteArray = new Float32Array(vertexArrayBuffer);
    vertexWriteArray.set([
        // x, y, z, w, r, g, b, a
        0, 0.8, 0, 1, 0, 1, 1, 1,
        -0.8, -0.8, 0, 1, 1, 1, 0, 1,
        0.8, -0.8, 0, 1, 1, 0, 1, 1
    ]);
    vertexBuffer.unmap();
    
    /*** Describe Vertex Data For Pipeline ***/
    
    const vertexBufferSlot = 0;
    
    /* GPUVertexAttributeDescriptors */
    const positionAttribute = {
        shaderLocation: positionLocation,
        inputSlot: vertexBufferSlot,
        offset: 0,
        format: "float4"
    };
    const colorAttribute = {
        shaderLocation: colorLocation,
        inputSlot: vertexBufferSlot,
        offset: colorOffset,
        format: "float4"
    };
    
    /* GPUVertexInputDescriptor */
    const vertexInputDescriptor = {
        inputSlot: vertexBufferSlot,
        stride: vertexStride,
        stepMode: "vertex"
    };
    
    /* GPUInputStateDescriptor */
    const inputStateDescriptor = {
        attributes: [positionAttribute, colorAttribute],
        inputs: [vertexInputDescriptor]
    };
    
    /*** Finish Pipeline State ***/
    
    /* GPUBlendDescriptors */
    const alphaBlendDescriptor = { srcFactor: "one", dstFactor: "zero", operation: "add" };
    const colorBlendDescriptor = { srcFactor: "one", dstFactor: "zero", operation: "add" };
    
    /* GPUColorStateDescriptor */
    const colorStateDescriptor = {
        format: "bgra8unorm",
        alphaBlend: alphaBlendDescriptor,
        colorBlend: colorBlendDescriptor,
        writeMask: GPUColorWriteBits.ALL
    };
    
    /* GPURenderPipelineDescriptor */
    const renderPipelineDescriptor = {
        vertexStage: vertexStageDescriptor,
        fragmentStage: fragmentStageDescriptor,
        primitiveTopology: "triangle-list",
        colorStates: [colorStateDescriptor],
        inputState: inputStateDescriptor
    };
    /* GPURenderPipeline */
    const renderPipeline = device.createRenderPipeline(renderPipelineDescriptor);
    
    /*** Swap Chain Setup ***/
    
    const canvas = document.querySelector("canvas");
    let canvasSize = canvas.getBoundingClientRect();
    canvas.width = canvasSize.width;
    canvas.height = canvasSize.height;

    const gpuContext = canvas.getContext("gpu");
    
    /* GPUSwapChainDescriptor */
    const swapChainDescriptor = { device: device, format: "bgra8unorm" };
    /* GPUSwapChain */
    const swapChain = gpuContext.configureSwapChain(swapChainDescriptor);
    
    /*** Render Pass Setup ***/
    
    /* Acquire Texture To Render To */
    
    /* GPUTexture */
    const swapChainTexture = swapChain.getCurrentTexture();
    /* GPUTextureView */
    const renderAttachment = swapChainTexture.createDefaultView();
    
    /* GPUColor */
    const darkBlue = { r: 0, g: 0, b: 0.5, a: 1 };
    
    /* GPURenderPassColorATtachmentDescriptor */
    const colorAttachmentDescriptor = {
        attachment: renderAttachment,
        loadOp: "clear",
        storeOp: "store",
        clearColor: darkBlue
    };
    
    /* GPURenderPassDescriptor */
    const renderPassDescriptor = { colorAttachments: [colorAttachmentDescriptor] };
    
    /*** Rendering ***/
    
    /* GPUCommandEncoder */
    const commandEncoder = device.createCommandEncoder();
    /* GPURenderPassEncoder */
    const renderPassEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
    
    renderPassEncoder.setPipeline(renderPipeline);
    renderPassEncoder.setVertexBuffers(vertexBufferSlot, [vertexBuffer], [0]);
    renderPassEncoder.draw(3, 1, 0, 0); // 3 vertices, 1 instance, 0th vertex, 0th instance.
    renderPassEncoder.endPass();
    
    /* GPUComamndBuffer */
    const commandBuffer = commandEncoder.finish();
    
    /* GPUQueue */
    const queue = device.getQueue();
    queue.submit([commandBuffer]);
}

window.addEventListener("DOMContentLoaded", helloTriangle);