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

    const whlslSource = `
    struct FragmentData {
        float4 position : SV_Position;
        float4 color : attribute(${colorLocation});
    }

    vertex FragmentData vertexMain(float4 position : attribute(${positionLocation}), float4 color : attribute(${colorLocation}))
    {
        FragmentData out;

        out.position = position;
        out.color = color;

        return out;
    }

    fragment float4 fragmentMain(float4 color : attribute(${colorLocation})) : SV_Target 0
    {
        return color;
    }
    `;
    const shaderModule = device.createShaderModule({ code: whlslSource, isWHLSL: true });
    
    /* GPUPipelineStageDescriptors */
    const vertexStageDescriptor = { module: shaderModule, entryPoint: "vertexMain" };
    const fragmentStageDescriptor = { module: shaderModule, entryPoint: "fragmentMain" };
    
    /*** Vertex Buffer Setup ***/
    
    /* Vertex Data */
    const colorOffset = 4 * 4; // 4 floats of 4 bytes each.
    const vertexStride = 8 * 4;
    const vertexDataSize = vertexStride * 3;
    
    /* GPUBufferDescriptor */
    const vertexDataBufferDescriptor = { 
        size: vertexDataSize,
        usage: GPUBufferUsage.MAP_WRITE | GPUBufferUsage.VERTEX
    };
    /* GPUBuffer */
    const vertexBuffer = device.createBuffer(vertexDataBufferDescriptor);
    
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
        offset: 0,
        format: "float4"
    };
    const colorAttribute = {
        shaderLocation: colorLocation,
        offset: colorOffset,
        format: "float4"
    };

    /* GPUVertexBufferDescriptor */
    const vertexBufferDescriptor = {
        stride: vertexStride,
        attributeSet: [positionAttribute, colorAttribute]
    };

    /* GPUVertexInputDescriptor */
    const vertexInputDescriptor = {
        vertexBuffers: [vertexBufferDescriptor]
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
        vertexInput: vertexInputDescriptor
    };
    /* GPURenderPipeline */
    const renderPipeline = device.createRenderPipeline(renderPipelineDescriptor);
    
    /*** Swap Chain Setup ***/
    
    const canvas = document.querySelector("canvas");
    canvas.width = 600;
    canvas.height = 600;

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
    const darkBlue = { r: 0.15, g: 0.15, b: 0.5, a: 1 };
    
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