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
    
    const uniformBufferSize = 16;
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
        usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT
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

    const shader = `
                override alphaMultiplier : f32;
                override betaMultiplier : f32;
                override gammaMultiplier : f32;
                override textureMultiplier : f32;
                override colorMultiplier : f32;
    
                struct VertexIn {
                   @location(0) position: vec4<f32>,
                   @location(1) color: vec4<f32>,
                   @location(2) uv: vec2<f32>,
                };
    
                struct VertexOut {
                   @builtin(position) position: vec4<f32>,
                   @location(0) color: vec4<f32>,
                   @location(1) uv: vec2<f32>,
                };
    
                struct Uniforms {
                    time: vec4<f32>,
                };
                @group(0) @binding(0) var<uniform> uniforms : Uniforms;
                @group(0) @binding(1) var colorTexture: texture_2d<f32>;
                @group(0) @binding(2) var textureSampler: sampler;
    
                @vertex fn vsmain(vin: VertexIn) -> VertexOut
                {
                    var vout : VertexOut;
                    let alpha = uniforms.time[0] * alphaMultiplier;
                    let beta = uniforms.time[1] * betaMultiplier;
                    let gamma = uniforms.time[2] * gammaMultiplier;
                    let cA = cos(alpha);
                    let sA = sin(alpha);
                    let cB = cos(beta);
                    let sB = sin(beta);
                    let cG = cos(gamma);
                    let sG = sin(gamma);
    
                    let m = mat4x4(cA * cB,  sA * cB,   -sB, 0,
                                          cA*sB*sG - sA*cG,  sA*sB*sG + cA*cG,   cB * sG, 0,
                                          cA*sB*cG + sA*sG, sA*sB*cG - cA*sG, cB * cG, 0,
                                          0,     0,     0, 1);
                    vout.position = vin.position * m;
                    vout.position.z = (vout.position.z + 0.5) * 0.5;
                    vout.color = vin.color;
                    vout.uv = vin.uv;
                    return vout;
                }

                @fragment fn fsmain(in: VertexOut) -> @location(0) vec4<f32>
                {
                    return colorMultiplier * in.color + textureMultiplier * vec4(textureSample(colorTexture, textureSampler, in.uv));
                }
    `;

    const shaderModule = device.createShaderModule({ code: shader });

    const milliseconds = (new Date()).getMilliseconds()
    
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
                    shaderLocation: 1,
                    offset: 4 * 4,
                    format: 'float32x4',
                },
                {
                    // uv
                    shaderLocation: 2,
                    offset: 4 * 8,
                    format: 'float32x2',
                },
              ],
            },
        ],
        constants: {
            alphaMultiplier: milliseconds % 2 + 1.0,
            betaMultiplier: milliseconds % 4 + 1.0,
            gammaMultiplier: milliseconds % 8 + 1.0
        },
    };

    const textureConstant = .1 * (milliseconds % 10 + 1.0);
    const fragmentStageDescriptor = {
        module: shaderModule,
        entryPoint: "fsmain",
        targets: [ {format: "bgra8unorm" }, ],
        constants: {
            textureMultiplier: textureConstant,
            colorMultiplier: 1.0 - textureConstant,
        },
    };

    /* GPURenderPipelineDescriptor */

    const renderPipelineDescriptor = {
        layout: 'auto',
        vertex: vertexStageDescriptor,
        fragment: fragmentStageDescriptor,
        primitive: {
            topology: "triangle-list",
            cullMode: "back"
        },
    };
    /* GPURenderPipeline */
    const renderPipeline = device.createRenderPipeline(renderPipelineDescriptor);

    /*** Shader Setup ***/

    const uniformBindGroup = device.createBindGroup({
        layout: renderPipeline.getBindGroupLayout(0),
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

    
    /*** Swap Chain Setup ***/
    function frameUpdate() {
        const secondsBuffer = new Float32Array(3);
        const d = new Date();
        const seconds = d.getMilliseconds() / 1000.0 + d.getSeconds();
        secondsBuffer.set([seconds*10 * (6.28318530718 / 60.0),
                          seconds*5 * (6.28318530718 / 60.0),
                          seconds*1 * (6.28318530718 / 60.0)]);
        device.queue.writeBuffer(uniformBuffer, 0, secondsBuffer);

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
