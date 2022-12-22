async function helloCube() {
    if (!navigator.gpu || GPUBufferUsage.COPY_SRC === undefined) {
        document.body.className = 'error';
        return;
    }

    const canvas = document.querySelector("canvas");
    canvas.width = 600;
    canvas.height = 600;
    
    const adapter = await navigator.gpu.requestAdapter();
    const device = await adapter.requestDevice();
    
    /*** Vertex Buffer Setup ***/
    
    /* Vertex Data */
    const vertexStride = 10 * 4;
    const vertexDataSize = vertexStride * 36;
    
    /* GPUBufferDfescriptor */
    const vertexDataBufferDescriptor = {
        size: vertexDataSize,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: true
    };

    /* GPUBuffer */
    const vertexBuffer = device.createBuffer(vertexDataBufferDescriptor);
    const vertexWriteArray = new Float32Array(vertexBuffer.getMappedRange());
    vertexWriteArray.set([
        // float4 position, float4 color, float2 uv
        .5, -.5, .5, 1,   1, 0, 1, 1,  1, 1,
        -.5, -.5, .5, 1,  0, 0, 1, 1,  0, 1,
        -.5, -.5, -.5, 1, 0, 0, 0, 1,  0, 0,
        .5, -.5, -.5, 1,  1, 0, 0, 1,  1, 0,
        .5, -.5, .5, 1,   1, 0, 1, 1,  1, 1,
        -.5, -.5, -.5, 1, 0, 0, 0, 1,  0, 0,

        .5, .5, .5, 1,    1, 1, 1, 1,  1, 1,
        .5, -.5, .5, 1,   1, 0, 1, 1,  0, 1,
        .5, -.5, -.5, 1,  1, 0, 0, 1,  0, 0,
        .5, .5, -.5, 1,   1, 1, 0, 1,  1, 0,
        .5, .5, .5, 1,    1, 1, 1, 1,  1, 1,
        .5, -.5, -.5, 1,  1, 0, 0, 1,  0, 0,

        -.5, .5, .5, 1,   0, 1, 1, 1,  1, 1,
        .5, .5, .5, 1,    1, 1, 1, 1,  0, 1,
        .5, .5, -.5, 1,   1, 1, 0, 1,  0, 0,
        -.5, .5, -.5, 1,  0, 1, 0, 1,  1, 0,
        -.5, .5, .5, 1,   0, 1, 1, 1,  1, 1,
        .5, .5, -.5, 1,   1, 1, 0, 1,  0, 0,

        -.5, -.5, .5, 1,  0, 0, 1, 1,  1, 1,
        -.5, .5, .5, 1,   0, 1, 1, 1,  0, 1,
        -.5, .5, -.5, 1,  0, 1, 0, 1,  0, 0,
        -.5, -.5, -.5, 1, 0, 0, 0, 1,  1, 0,
        -.5, -.5, .5, 1,  0, 0, 1, 1,  1, 1,
        -.5, .5, -.5, 1,  0, 1, 0, 1,  0, 0,

        .5, .5, .5, 1,    1, 1, 1, 1,  1, 1,
        -.5, .5, .5, 1,   0, 1, 1, 1,  0, 1,
        -.5, -.5, .5, 1,  0, 0, 1, 1,  0, 0,
        -.5, -.5, .5, 1,  0, 0, 1, 1,  0, 0,
        .5, -.5, .5, 1,   1, 0, 1, 1,  1, 0,
        .5, .5, .5, 1,    1, 1, 1, 1,  1, 1,
    
        .5, -.5, -.5, 1,  1, 0, 0, 1,  1, 1,
        -.5, -.5, -.5, 1, 0, 0, 0, 1,  0, 1,
        -.5, .5, -.5, 1,  0, 1, 0, 1,  0, 0,
        .5, .5, -.5, 1,   1, 1, 0, 1,  1, 0,
        .5, -.5, -.5, 1,  1, 0, 0, 1,  1, 1,
        -.5, .5, -.5, 1,  0, 1, 0, 1,  0, 0,
    ]);
    vertexBuffer.unmap();
    
    const uniformBufferSize = 12;
    const uniformBuffer = device.createBuffer({
        size: uniformBufferSize,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    
    /* GPUTexture */
    const image = new Image();
    const imageLoadPromise = new Promise(resolve => {
        image.onload = () => resolve();
        image.src = "resources/webkit-logo.png"
    });
    await Promise.resolve(imageLoadPromise);

    const textureSize = {
        width: image.width,
        height: image.height,
        depth: 1
    };

    const textureDescriptor = {
        size: textureSize,
        arrayLayerCount: 1,
        mipLevelCount: 1,
        sampleCount: 1,
        dimension: "2d",
        format: "bgra8unorm",
        usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST
    };
    const texture = device.createTexture(textureDescriptor);
    
    const imageBitmap = await createImageBitmap(image);

    device.queue.copyExternalImageToTexture(
        { source: imageBitmap },
        { texture: texture },
        [image.width, image.height]
    );

    const sampler = device.createSampler({
        magFilter: "linear",
        minFilter: "linear"
    });

    /*** Shader Setup ***/
    
    const uniformBindGroupLayout = device.createBindGroupLayout({ entries: [
        { binding: 0, visibility: GPUShaderStage.VERTEX, buffer: {} },
        { binding: 1, visibility: GPUShaderStage.FRAGMENT, texture: {} },
        { binding: 2, visibility: GPUShaderStage.FRAGMENT, sampler: {} },
    ] });

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
          {
            binding: 1,
            resource: texture.createView(),
          },
          {
            binding: 2,
            resource: sampler,
          },
        ],
      });

    const mslSource = `
                #define vertexInputPackedFloatSize 10
                #include <metal_stdlib>
                using namespace metal;
                struct VertexIn {
                   float4 position [[attribute(0)]];
                   float4 color [[attribute(1)]];
                   float2 uv [[attribute(2)]];
                };
    
                struct VertexOut {
                   float4 position [[position]];
                   float4 color;
                   float2 uv;
                };
    
                struct VertexShaderArguments {
                    device float *time [[id(0)]];
                };
    
                struct FragmentShaderArguments {
                    texture2d<half> colorTexture;
                    sampler textureSampler;
                };
    
                vertex VertexOut vsmain(VertexIn vin [[stage_in]], const device VertexShaderArguments &values [[buffer(1)]])
                {
                    VertexOut vout;
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
                    vout.position = vin.position * m;
                    vout.position.z = (vout.position.z + 0.5) * 0.5;
                    vout.color = vin.color;
                    vout.uv = vin.uv;
                    return vout;
                }

                fragment float4 fsmain(VertexOut in [[stage_in]], device FragmentShaderArguments &values [[buffer(0)]])
                {
                    return 0.5 * in.color + 0.5 * float4(values.colorTexture.sample(values.textureSampler, in.uv));
                }
    `;

    const shaderModule = device.createShaderModule({ code: mslSource, isWHLSL: false, hints: [ {layout: "auto" }, ] });
    
    /* GPUPipelineStageDescriptors */
    const vertexStageDescriptor = {
        module: shaderModule,
        entryPoint: "vsmain",
        buffers: [
            {
                arrayStride: vertexStride,
                attributes: [
                {
                    // position
                    shaderLocation: 0,
                    offset: 0,
                    format: 'float32x4',
                },
                {
                    // color
                    shaderLocation: 0,
                    offset: 4 * 4,
                    format: 'float32x4',
                },
                {
                    // uv
                    shaderLocation: 1,
                    offset: 4 * 8,
                    format: 'float32x2',
                },
              ],
            },
        ],
    };

    const fragmentStageDescriptor = { module: shaderModule, entryPoint: "fsmain", targets: [ {format: "bgra8unorm" }, ],  };

    /* GPURenderPipelineDescriptor */

    const renderPipelineDescriptor = {
        layout: "auto",
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
        device.queue.writeBuffer(uniformBuffer, 0, secondsBuffer, 0, 12);

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
        renderPassEncoder.setVertexBuffer(0, vertexBuffer, 0);
        renderPassEncoder.setBindGroup(0, uniformBindGroup);
        renderPassEncoder.draw(36, 1, 0, 0); // 36 vertices, 1 instance, 0th vertex, 0th instance.
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
