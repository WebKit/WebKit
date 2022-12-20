async function helloCube() {
    if (!navigator.gpu || GPUBufferUsage.COPY_SRC === undefined) {
        document.body.className = 'error';
        return;
    }

    const adapter = await navigator.gpu.requestAdapter();
    const device = await adapter.requestDevice();
    
    /*** Vertex Buffer Setup ***/
    
    /* Vertex Data */
    const vertexStride = 8 * 4;
    const vertexDataSize = vertexStride * 8;
    
    /* GPUBufferDescriptor */
    const vertexDataBufferDescriptor = {
        size: vertexDataSize,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: true
    };

    const indexDataSize = 2 * 36;

    const indexDataBufferDescriptor = {
        size: indexDataSize,
        usage: GPUBufferUsage.INDEX,
        mappedAtCreation: true
    };

    /* GPUBuffer instances */
    const vertexBuffer = device.createBuffer(vertexDataBufferDescriptor);
    const vertexWriteArray = new Float32Array(vertexBuffer.getMappedRange());
    vertexWriteArray.set([
        // float4 position, float4 color
        -0.5, -0.5, 0.5, 1,   1, 0, 1, 1, // 0
        0.5, -0.5, 0.5, 1,  0, 0, 1, 1, // 1
        0.5, 0.5, 0.5, 1, 0, 0, 0, 1, // 2
        -0.5, 0.5, 0.5, 1,  1, 0, 0, 1, // 3
        -0.5, -0.5, -0.5, 1,    1, 1, 1, 1, // 4
        0.5, -0.5, -0.5, 1,   1, 1, 0, 1, // 5
        0.5, 0.5, -0.5, 1,   0, 1, 1, 1, // 6
        -0.5, 0.5, -0.5, 1,  0, 1, 0, 1, // 7
    ]);
    vertexBuffer.unmap();
    
    const indexBuffer = device.createBuffer(indexDataBufferDescriptor);
    const indexWriteArray = new Int16Array(indexBuffer.getMappedRange());
    indexWriteArray.set([
        0, 1, 2,
        2, 3, 0,
        1, 5, 6,
        6, 2, 1,
        7, 6, 5,
        5, 4, 7,
        4, 0, 3,
        3, 7, 4,
        4, 5, 1,
        1, 0, 4,
        3, 2, 6,
        6, 7, 3
    ]);
    indexBuffer.unmap();
    
    const uniformBufferSize = 12;
    const uniformBuffer = device.createBuffer({
        size: uniformBufferSize,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

    /*** Shader Setup ***/
    
    const uniformBindGroupLayout = device.createBindGroupLayout({ entries: [{binding: 0, visibility: GPUShaderStage.VERTEX, buffer: {}}] });
    const uniformBindGroup = device.createBindGroup({
        layout: uniformBindGroupLayout,
        entries: [
          {
            binding: 0,
            resource: {
              buffer: uniformBuffer,
              offset: 0
            },
          },
        ],
      });
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
    
                struct VertexShaderArguments {
                    device float *time [[id(0)]];
                };
    
                vertex Vertex vsmain(device Vertex *vertices [[buffer(0)]], device VertexShaderArguments &values [[buffer(1)]], unsigned VertexIndex [[vertex_id]])
                {
                    Vertex vout;
                    float alpha = values.time[0];
                    float beta = values.time[1];
                    float gamma = values.time[2];
                    float cA = cos(alpha);
                    float sA = sin(alpha);
                    float cB = cos(beta);
                    float sB = sin(beta);
                    float cG = cos(gamma);
                    float sG = sin(gamma);
    
                    float4x4 m = float4x4(cA * cB,  sA * cB,   -sB, 0,
                                          cA*sB*sG - sA*cG,  sA*sB*sG + cA*cG,   cB * sG, 0,
                                             cA*sB*cG + sA*sG, sA*sB*cG - cA*sG, cB * cG, 0,
                                              0,     0,     0, 1);
                    vout.position = vertices[VertexIndex].position * m;
                    vout.position.z = (vout.position.z + 0.5) * 0.5;
                    vout.color = vertices[VertexIndex].color;
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
        primitive: {
            topology: "triangle-list",
            cullMode: "back"
        },
    };
    /* GPURenderPipeline */
    const renderPipeline = device.createRenderPipeline(renderPipelineDescriptor);
    
    /*** Swap Chain Setup ***/
    function frameUpdate() {
        const secondsBuffer = new Float32Array(3);
        const d = new Date();
        const seconds = d.getMilliseconds() / 1000.0 + d.getSeconds();
        secondsBuffer.set([seconds*10 * (6.28318530718 / 60.0),
                          seconds*5 * (6.28318530718 / 60.0),
                          seconds*1 * (6.28318530718 / 60.0)]);
        // document.writeln(d.getSeconds());
        device.queue.writeBuffer(uniformBuffer, 0, secondsBuffer, 0, 12);

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
        renderPassEncoder.setIndexBuffer(indexBuffer, "uint16", 0);
        renderPassEncoder.setBindGroup(1, uniformBindGroup);
        renderPassEncoder.drawIndexed(36, 1, 0, 0, 0); // 36 indices, 1 instance, 0th index, 0th vertex, 0th instance.
        renderPassEncoder.end();
        
        /* GPUComamndBuffer */
        const commandBuffer = commandEncoder.finish();
        
        /* GPUQueue */
        const queue = device.queue;
        queue.submit([commandBuffer]);

        requestAnimationFrame(frameUpdate);
    }

    requestAnimationFrame(frameUpdate);
}

window.addEventListener("DOMContentLoaded", helloCube);
