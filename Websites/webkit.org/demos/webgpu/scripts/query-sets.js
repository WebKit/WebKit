async function helloTriangle() {
    if (!navigator.gpu || GPUBufferUsage.COPY_SRC === undefined) {
        document.body.className = 'error';
        return;
    }
    
    const adapter = await navigator.gpu.requestAdapter();
    const device = await adapter.requestDevice();
    
    /*** Query Set creation ***/
    const timestampQueryCount = 4;
    const timestampQuery = device.createQuerySet({
        type: 'timestamp',
        count: timestampQueryCount
    });

    const timestampQueryBufferSize = timestampQueryCount * 8;
    const timestampQueryResults = device.createBuffer({
        size: timestampQueryBufferSize,
        usage: GPUBufferUsage.QUERY_RESOLVE | GPUBufferUsage.COPY_SRC
    });
    
    const timestampQueryDisplayResults = device.createBuffer({
        size: timestampQueryBufferSize,
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ
    });
    
    const occlusionQueryCount = 4;
    const occlusionQuery = device.createQuerySet({
        type: 'occlusion',
        count: occlusionQueryCount
    });
    
    const occlusionQueryBufferSize = 8 * occlusionQueryCount;
    const occlusionQueryResults = device.createBuffer({
        size: occlusionQueryBufferSize,
        usage: GPUBufferUsage.QUERY_RESOLVE | GPUBufferUsage.COPY_SRC
    });
    
    const occlusionQueryDisplayResults = device.createBuffer({
        size: occlusionQueryBufferSize,
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ
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
    const uniformBindGroupLayout = device.createBindGroupLayout({ entries: [{binding: 0, visibility: GPUShaderStage.VERTEX, buffer: {}}] });
    const pipelineLayoutDesc = { bindGroupLayouts: [uniformBindGroupLayout] };
    const layout = device.createPipelineLayout(pipelineLayoutDesc);
    const uniformBufferSize = 12;
    const uniformBuffer = device.createBuffer({
        size: uniformBufferSize,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

    const uniformBindGroup = device.createBindGroup({
        layout: uniformBindGroupLayout,
        entries: [
          {
            binding: 0,
            resource: {
              buffer: uniformBuffer,
              offset: 0
            },
          }
        ]
    });

    const mslSource = `
                #include <metal_stdlib>
                using namespace metal;
                struct Vertex {
                   float4 position [[position]];
                   float4 color;
                };
    
                struct VertexShaderArguments {
                    device float *time [[id(0)]];
                };

                vertex Vertex vsmain(const device VertexShaderArguments &values [[buffer(1)]], unsigned VertexIndex [[vertex_id]])
                {
                   float2 pos[3] = {
                       float2( 0.0,  0.5 + .05 * values.time[0]),
                       float2(-0.5 -  .05 * values.time[1], -0.5 -  .05 * values.time[1]),
                       float2( 0.5 +  .05 * values.time[2], -0.5 -  .05 * values.time[2]),
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
    
    const shaderModule = device.createShaderModule({ code: mslSource, isWHLSL: false, hints: [ {layout: "auto" }, ] });
    
    /* GPUPipelineStageDescriptors */
    const vertexStageDescriptor = { module: shaderModule, entryPoint: "vsmain" };
    
    const fragmentStageDescriptor = { module: shaderModule, entryPoint: "fsmain", targets: [ {format: "bgra8unorm" }, ],  };
    
    /* GPURenderPipelineDescriptor */
    
    const renderPipelineDescriptor = {
        layout: "auto",
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
    async function frameUpdate() {
        const secondsBuffer = new Float32Array(3);
        const d = new Date();
        const seconds = d.getMilliseconds() / 1000 + d.getSeconds();
        secondsBuffer.set([seconds*10 * (6.28318530718 / 60.0),
                          seconds*5 * (6.28318530718 / 60.0),
                          seconds*1 * (6.28318530718 / 60.0)]);
        device.queue.writeBuffer(uniformBuffer, 0, secondsBuffer, 0, 12);

        const currentTexture = gpuContext.getCurrentTexture();
        
        /*** Render Pass Setup ***/
        
        /* Acquire Texture To Render To */
        
        /* GPUTextureView */
        const renderAttachment = currentTexture.createView();
        
        /* GPUColor */
        const black = { r: 0.0, g: 0.0, b: 0.0, a: 1 };
        
        /* GPURenderPassColorATtachmentDescriptor */
        const colorAttachmentDescriptor = {
            view: renderAttachment,
            loadOp: "clear",
            storeOp: "store",
            clearValue: black
        };
        
        /* GPURenderPassDescriptor */
        const renderPassDescriptor = {
            colorAttachments: [colorAttachmentDescriptor],
            occlusionQuerySet: occlusionQuery,
            timestampWrites: [ { querySet: timestampQuery, queryIndex: 0, location: "beginning" },
                               { querySet: timestampQuery, queryIndex: 1, location: "end" } ]
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
        renderPassEncoder.beginOcclusionQuery(0);
        renderPassEncoder.draw(3, 1, 0, 0); // 3 vertices, 1 instance, 0th vertex, 0th instance.
        renderPassEncoder.endOcclusionQuery();
        renderPassEncoder.end();
        
        commandEncoder.resolveQuerySet(occlusionQuery, 0, occlusionQueryCount, occlusionQueryResults, 0); // querySet, firstQuery, queryCount, destination, destinationOffset
        commandEncoder.resolveQuerySet(timestampQuery, 0, timestampQueryCount, timestampQueryResults, 0); // querySet, firstQuery, queryCount, destination, destinationOffset

        /* GPUComamndBuffer */
        const commandBuffer = commandEncoder.finish();
        
        /* GPUQueue */
        const queue = device.queue;
        queue.submit([commandBuffer]);
        
        const copyBufferEncoder = device.createCommandEncoder();
        copyBufferEncoder.copyBufferToBuffer(occlusionQueryResults, 0, occlusionQueryDisplayResults, 0, occlusionQueryBufferSize);
        copyBufferEncoder.copyBufferToBuffer(timestampQueryResults, 0, timestampQueryDisplayResults, 0, timestampQueryBufferSize);
        const copyBufferEncoderCommandBuffe = copyBufferEncoder.finish();
        queue.submit([copyBufferEncoderCommandBuffe]);

        const waitForWorkCompletion = await queue.onSubmittedWorkDone();

        const occlusionQueryResultsArrayBuffer = await occlusionQueryDisplayResults.mapAsync(GPUMapMode.READ);
        const occlusionQueryResultsArray = new BigUint64Array(occlusionQueryDisplayResults.getMappedRange());
        console.log("Occlusion query results: " + occlusionQueryResultsArray);
        occlusionQueryDisplayResults.unmap();
        
        const timestampQueryResultsArrayBuffer = await timestampQueryDisplayResults.mapAsync(GPUMapMode.READ);
        const timestampQueryResultsArray = new BigUint64Array(timestampQueryDisplayResults.getMappedRange());
        console.log("Timestamp query results: " + timestampQueryResultsArray);
        timestampQueryDisplayResults.unmap();
    
        requestAnimationFrame(frameUpdate);
    }
    
    requestAnimationFrame(frameUpdate);
}

window.addEventListener("DOMContentLoaded", helloTriangle);
