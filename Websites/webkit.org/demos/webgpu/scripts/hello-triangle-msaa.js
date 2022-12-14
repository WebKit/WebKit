async function helloTriangle() {
    if (!navigator.gpu || GPUBufferUsage.COPY_SRC === undefined) {
        document.body.className = 'error';
        return;
    }

    const canvas = document.querySelector("canvas");
    canvas.width = 600;
    canvas.height = 600;

    const adapter = await navigator.gpu.requestAdapter();
    const device = await adapter.requestDevice();
    
    const msaaRenderTarget = device.createTexture({
        size: [ canvas.width, canvas.height ],
        sampleCount: 4,
        format: 'bgra8unorm',
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
    });

    /*** Vertex Buffer Setup ***/
    
    /* Vertex Data */
    const vertexStride = 8 * 4;
    const vertexDataSize = vertexStride * 3;
    
    /* GPUBufferDescriptor */
    const vertexDataBufferDescriptor = {
        size: vertexDataSize,
        usage: GPUBufferUsage.VERTEX
    };

    /* GPUBuffer */
    const vertexBuffer = device.createBuffer(vertexDataBufferDescriptor);
    
    /*** Shader Setup ***/
    const mtlSource = `
                #include <metal_stdlib>
                using namespace metal;
                struct Vertex {
                   float4 position [[position]];
                   float4 color;
                };

                vertex Vertex vsmain(unsigned VertexIndex [[vertex_id]])
                {
                   float2 pos[3] = {
                       float2( 0.0,  1.0),
                       float2(-1.0, -1.0),
                       float2( 1.0, -1.0),
                   };

                   Vertex vout;
                   vout.position = float4(pos[VertexIndex], 0.0, 1.0);
                   vout.color = float4(pos[VertexIndex] + float2(0.5, 0.5), 0.0, 1.0);
                   return vout;
                }

                fragment float4 fsmain(Vertex in [[stage_in]])
                {
                    return in.color;
                }
    `;

    const shaderModule = device.createShaderModule({ code: mtlSource, isWHLSL: false, hints: [ {layout: "auto" }, ] });
    
    /* GPUPipelineStageDescriptors */
    const vertexStageDescriptor = { module: shaderModule, entryPoint: "vsmain" };

    const fragmentStageDescriptor = { module: shaderModule, entryPoint: "fsmain", targets: [ {format: "bgra8unorm" }, ],  };

    /* GPURenderPipelineDescriptor */

    const renderPipelineDescriptor = {
        layout: "auto",
        vertex: vertexStageDescriptor,
        fragment: fragmentStageDescriptor,
        primitive: { topology: "triangle-list" },
        multisample: { count: 4 }
    };
    /* GPURenderPipeline */
    const renderPipeline = device.createRenderPipeline(renderPipelineDescriptor);
    
    /*** Swap Chain Setup ***/
    
    const gpuContext = canvas.getContext("webgpu");
    
    /* GPUCanvasConfiguration */
    const canvasConfiguration = { device: device, format: "bgra8unorm" };
    gpuContext.configure(canvasConfiguration);
    /* GPUTexture */
    const currentTexture = gpuContext.getCurrentTexture();
    
    /*** Render Pass Setup ***/
    
    /* Acquire Texture To Render To */
    
    /* GPUTextureView */
    const renderAttachment = currentTexture.createView();
    
    /* GPUColor */
    const darkBlue = { r: 0.15, g: 0.15, b: 0.5, a: 1 };
    
    /* GPURenderPassColorAttachment */
    const colorAttachmentDescriptor = {
        view: msaaRenderTarget.createView(),
        resolveTarget: renderAttachment,
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
    const vertexBufferSlot = 0;
    renderPassEncoder.setVertexBuffer(vertexBufferSlot, vertexBuffer, 0);
    renderPassEncoder.draw(3, 1, 0, 0); // 3 vertices, 1 instance, 0th vertex, 0th instance.
    renderPassEncoder.end();
    
    /* GPUComamndBuffer */
    const commandBuffer = commandEncoder.finish();
    
    /* GPUQueue */
    const queue = device.queue;
    queue.submit([commandBuffer]);
}

window.addEventListener("DOMContentLoaded", helloTriangle);
