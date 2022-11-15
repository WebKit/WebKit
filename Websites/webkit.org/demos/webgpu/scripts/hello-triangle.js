async function helloTriangle() {
    if (!navigator.gpu || GPUBufferUsage.COPY_SRC === undefined) {
        document.body.className = 'error';
        return;
    }

    const adapter = await navigator.gpu.requestAdapter();
    const device = await adapter.requestDevice();
    
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
    
    const uniformBindGroupLayout = device.createBindGroupLayout({ entries: [{binding: 0, visibility: GPUShaderStage.VERTEX, buffer: {}}] });
    const pipelineLayoutDesc = { bindGroupLayouts: [uniformBindGroupLayout] };
    const layout = device.createPipelineLayout(pipelineLayoutDesc);
/*
    FIXME: Use WGSL once compiler is brought up
    const wgslSource = `
                     @vertex fn main(@builtin(vertex_index) VertexIndex: u32) -> @builtin(position) vec4<f32>
                     {
                         var pos = array<vec2<f32>, 3>(
                             vec2<f32>( 0.0,  0.5),
                             vec2<f32>(-0.5, -0.5),
                             vec2<f32>( 0.5, -0.5)
                         );
                         return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
                     }

                     @fragment fn main() -> @location(0) vec4<f32>
                     {
                         return vec4<f32>(1.0, 0.0, 0.0, 1.0);
                     }
    `;
 */
    const wgslSource = `
                #include <metal_stdlib>
                using namespace metal;
                struct Vertex {
                   float4 position [[position]];
                   float4 color;
                };

                vertex Vertex vsmain(unsigned VertexIndex [[vertex_id]])
                {
                   float2 pos[3] = {
                       float2( 0.0,  0.5),
                       float2(-0.5, -0.5),
                       float2( 0.5, -0.5),
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

    const shaderModule = device.createShaderModule({ code: wgslSource, isWHLSL: false, hints: [ {layout: layout }, ] });
    
    /* GPUPipelineStageDescriptors */
    const vertexStageDescriptor = { module: shaderModule, entryPoint: "vsmain" };

    const fragmentStageDescriptor = { module: shaderModule, entryPoint: "fsmain", targets: [ {format: "bgra8unorm" }, ],  };
    
    /* GPURenderPipelineDescriptor */

    const renderPipelineDescriptor = {
        layout: layout,
        vertex: vertexStageDescriptor,
        fragment: fragmentStageDescriptor,
        primitive: {topology: "triangle-list" },
    };
    /* GPURenderPipeline */
    const renderPipeline = device.createRenderPipeline(renderPipelineDescriptor);
    
    /*** Swap Chain Setup ***/
    
    const canvas = document.querySelector("canvas");
    canvas.width = 600;
    canvas.height = 600;

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
    
    /* GPURenderPassColorATtachmentDescriptor */
    const colorAttachmentDescriptor = {
        view: renderAttachment,
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
