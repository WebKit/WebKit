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

    const instanceRows = 3;
    const instanceColumns = 3;
    const instanceCount = instanceRows * instanceColumns;
    const uniformBufferSize = 12 * instanceCount;
    const uniformBuffer = device.createBuffer({
        size: uniformBufferSize,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    
    const translationBufferSize = 4 * 4 * instanceCount;
    const translationBuffer = device.createBuffer({
        size: translationBufferSize,
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

    const mslSource = `
                #define vertexInputPackedFloatSize 10
                #include <metal_stdlib>
                using namespace metal;
                struct VertexIn {
                   float4 position;
                   float4 color;
                   float2 uv;
                };
    
                struct VertexOut {
                   float4 position [[position]];
                   float4 color;
                   float2 uv;
                };
    
                struct VertexShaderArguments {
                    device float *time [[id(0)]];
                    device float4 *translation [[id(1)]];
                };
    
                struct FragmentShaderArguments {
                    texture2d<half> colorTexture;
                    sampler textureSampler;
                };
    
                vertex VertexOut vsmain(const device float *vertices [[buffer(0)]], const device VertexShaderArguments &values [[buffer(1)]], unsigned VertexIndex [[vertex_id]], unsigned InstanceIndex [[instance_id]])
                {
                    VertexOut vout;
                    unsigned offset = InstanceIndex * 3;
                    float alpha = values.time[offset];
                    float beta = values.time[offset + 1];
                    float gamma = values.time[offset + 2];
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
                    float4 translation = values.translation[InstanceIndex];

                    VertexIn vin = *(device VertexIn*)(vertices + VertexIndex * vertexInputPackedFloatSize);
                    vout.position = vin.position * m + translation;
                    vout.position.z = 1 - (vout.position.z + 0.5) * 0.5;
                    vout.color = vin.color;
                    vout.uv = vin.uv;
                    return vout;
                }

                fragment float4 fsmain(VertexOut in [[stage_in]], device FragmentShaderArguments &values [[buffer(1)]])
                {
                    return 0.5 * in.color + 0.5 * float4(values.colorTexture.sample(values.textureSampler, in.uv));
                }
    `;

    const shaderModule = device.createShaderModule({ code: mslSource, isWHLSL: false, hints: [ {layout: "auto" }, ] });
    
    /* GPUPipelineStageDescriptors */
    const vertexStageDescriptor = { module: shaderModule, entryPoint: "vsmain" };

    const fragmentStageDescriptor = { module: shaderModule, entryPoint: "fsmain", targets: [ {format: "bgra8unorm" }, ],  };

    /* GPURenderPipelineDescriptor */

    const renderPipelineDescriptor = {
        layout: "auto",
        vertex: vertexStageDescriptor,
        fragment: fragmentStageDescriptor,
        primitive: {
            topology: "triangle-list",
            cullMode: "none"
        },
        depthStencil: {
            format: "depth24plus",
            depthWriteEnabled: true,
            depthCompare: "less-equal"
        },
        multisample: { count: 4 }
    };

    const deviceScaleFactor = window.devicePixelRatio || 1;
    const depthTexture = device.createTexture({
        size: [ canvas.width * deviceScaleFactor, canvas.height * deviceScaleFactor ],
        format: 'depth24plus',
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
        sampleCount: 4
    });

    const msaaRenderTarget = device.createTexture({
        size: [ canvas.width * deviceScaleFactor, canvas.height * deviceScaleFactor ],
        sampleCount: 4,
        format: 'bgra8unorm',
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const depthTextureView = depthTexture.createView();

    /* GPURenderPipeline */
    const renderPipeline = device.createRenderPipeline(renderPipelineDescriptor);

    const uniformBindGroup = device.createBindGroup({
        layout: renderPipeline.getBindGroupLayout(1),
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
            resource: {
              buffer: translationBuffer,
              offset: 0
            },
          },
          {
            binding: 2,
            resource: texture.createView(),
          },
          {
            binding: 3,
            resource: sampler,
          },
        ],
      });

    /*** Swap Chain Setup ***/
    function frameUpdate() {
        const d = new Date();
        const seconds = d.getMilliseconds() / 1000.0 + d.getSeconds();
        const secondsBuffer = new Float32Array(instanceCount * 3);
        const translations = new Float32Array(instanceCount * 4);
        const pi = 6.28318530718;
        for (let i = 0; i < instanceCount; ++i) {
            const offset = (pi * i) / instanceCount;
            const speed = 2;
            secondsBuffer[i * 3] = seconds * speed * (pi / 60.0) + offset;
            secondsBuffer[i * 3 + 1] = seconds * (speed * 0.5) * (pi / 60.0) + offset;
            secondsBuffer[i * 3 + 2] = seconds * (speed * 0.1) * (pi / 60.0) + offset;

            const x = i % instanceColumns;
            const y = i / instanceColumns;

            const xOffset = Math.floor((x - (instanceColumns - 1) / 2));
            const yOffset = Math.floor((y - (instanceColumns - 1) / 2));
            translations[i * 4] = xOffset;
            translations[i * 4 + 1] = yOffset;
            translations[i * 4 + 2] = 0;
            translations[i * 4 + 3] = 1;
        }

        device.queue.writeBuffer(uniformBuffer, 0, secondsBuffer, 0, uniformBufferSize);
        device.queue.writeBuffer(translationBuffer, 0, translations, 0, translationBufferSize);

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
            view: msaaRenderTarget.createView(),
            resolveTarget: renderAttachment,
            loadOp: "clear",
            storeOp: "store",
            clearColor: darkBlue
        };
        
        /* GPURenderPassDescriptor */
        const depthAttachment = {
            view: depthTextureView,
            depthClearValue: 1.0,
            depthLoadOp: "clear",
            depthStoreOp: "store",
            stencilLoadOp: "clear",
            stencilStoreOp: "store",
        };
        const renderPassDescriptor = {
            colorAttachments: [colorAttachmentDescriptor],
            depthStencilAttachment: depthAttachment
        };
        
        /*** Rendering ***/
        
        /* GPUCommandEncoder */
        const commandEncoder = device.createCommandEncoder();
        /* GPURenderPassEncoder */
        const renderPassEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
        
        renderPassEncoder.setPipeline(renderPipeline);
        const vertexBufferSlot = 0;
        renderPassEncoder.setVertexBuffer(vertexBufferSlot, vertexBuffer, 0);
        renderPassEncoder.setBindGroup(1, uniformBindGroup);
        renderPassEncoder.draw(36, instanceCount, 0, 0); // 36 vertices, 10 instances, 0th vertex, 0th instance.
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
