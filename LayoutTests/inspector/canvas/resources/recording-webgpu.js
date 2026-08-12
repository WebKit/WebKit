let device = null;
let context = null;
let format = null;
let videoFrame = null;

let bindGroup = null;
let bindGroupLayout = null;
let buffer = null;
let commandBuffer = null;
let commandEncoder = null;
let computePassEncoder = null;
let computePipeline = null;
let externalTexture = null;
let pipelineLayout = null;
let querySet = null;
let queue = null;
let renderBundle = null;
let renderBundleEncoder = null;
let renderPassEncoder = null;
let renderPipeline = null;
let sampler = null;
let shaderModule = null;
let texture = null;
let textureView = null;

const shaderModuleCode = `
    @vertex fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f {
        let x = f32(i32(vertexIndex) - 1);
        let y = f32(i32(vertexIndex & 1u) * 2 - 1);
        return vec4f(x, y, 0, 1);
    }

    @fragment fn fragmentMain() -> @location(0) vec4f {
        return vec4f(1, 0, 0, 1);
    }

    @compute @workgroup_size(1) fn computeMain() {
    }
`;

function ignoreException(callback) {
    try {
        callback();
    } catch { }
}

function createRenderPipelineDescriptor() {
    return {
        layout: pipelineLayout,
        vertex: {module: shaderModule, entryPoint: "vertexMain"},
        fragment: {
            module: shaderModule,
            entryPoint: "fragmentMain",
            targets: [{format}],
        },
    };
}

function createComputePipelineDescriptor() {
    return {
        layout: pipelineLayout,
        compute: {module: shaderModule, entryPoint: "computeMain"},
    };
}

function createBufferDescriptor() {
    return {
        size: 256,
        usage: GPUBufferUsage.COPY_SRC
            | GPUBufferUsage.COPY_DST
            | GPUBufferUsage.INDEX
            | GPUBufferUsage.VERTEX
            | GPUBufferUsage.UNIFORM
            | GPUBufferUsage.INDIRECT
            | GPUBufferUsage.QUERY_RESOLVE,
    };
}

function createTextureDescriptor() {
    return {
        size: {width: 4, height: 4},
        format,
        usage: GPUTextureUsage.COPY_SRC
            | GPUTextureUsage.COPY_DST
            | GPUTextureUsage.TEXTURE_BINDING
            | GPUTextureUsage.RENDER_ATTACHMENT,
    };
}

function createRenderPassDescriptor() {
    return {
        colorAttachments: [{
            view: textureView,
            resolveTarget: texture,
            clearValue: {r: 0, g: 0, b: 0, a: 1},
            loadOp: "clear",
            storeOp: "store",
        }, null, {
            view: texture,
            depthSlice: 0,
            resolveTarget: textureView,
            clearValue: [1, 0, 0, 1],
            loadOp: "load",
            storeOp: "store",
        }],
        depthStencilAttachment: {
            view: textureView,
            depthClearValue: 1,
            depthLoadOp: "clear",
            depthStoreOp: "store",
            depthReadOnly: true,
            stencilClearValue: 1,
            stencilLoadOp: "clear",
            stencilStoreOp: "store",
            stencilReadOnly: true,
        },
        occlusionQuerySet: querySet,
        timestampWrites: {
            querySet,
            beginningOfPassWriteIndex: 0,
            endOfPassWriteIndex: 1,
        },
        maxDrawCount: 1,
    };
}

let requestAnimationFrameId = NaN;

function cancelActions() {
    cancelAnimationFrame(requestAnimationFrameId);
    requestAnimationFrameId = NaN;
}

function performActions() {
    // Keep this list in sync with WebGPU interfaces that use `CallTracer`.
    // Each frame exercises one action name and groups its overloads.
    let frames = [
        () => {
            device.label = "device";
            device.label;
        },
        () => {
            device.pushErrorScope("validation");
        },
        () => device.popErrorScope().catch(() => { }),
        () => {
            buffer = device.createBuffer(createBufferDescriptor());
        },
        () => {
            texture = device.createTexture(createTextureDescriptor());
        },
        () => {
            sampler = device.createSampler({});
        },
        () => {
            externalTexture = device.importExternalTexture({source: videoFrame});
        },
        () => {
            bindGroupLayout = device.createBindGroupLayout({
                entries: [{
                    binding: 0,
                    visibility: GPUShaderStage.VERTEX | GPUShaderStage.FRAGMENT | GPUShaderStage.COMPUTE,
                    buffer: {},
                }],
            });
        },
        () => {
            pipelineLayout = device.createPipelineLayout({bindGroupLayouts: [bindGroupLayout, null]});
        },
        () => {
            bindGroup = device.createBindGroup({
                layout: bindGroupLayout,
                entries: [{binding: 0, resource: {buffer}}],
            });
        },
        () => {
            shaderModule = device.createShaderModule({
                code: shaderModuleCode,
                sourceMap: {opaque: true},
                hints: {
                    computeMain: {layout: "auto"},
                    vertexMain: {layout: pipelineLayout},
                },
            });
        },
        () => {
            computePipeline = device.createComputePipeline(createComputePipelineDescriptor());
        },
        () => {
            renderPipeline = device.createRenderPipeline(createRenderPipelineDescriptor());
        },
        () => device.createComputePipelineAsync(createComputePipelineDescriptor()).catch(() => { }),
        () => device.createRenderPipelineAsync(createRenderPipelineDescriptor()).catch(() => { }),
        () => {
            commandEncoder = device.createCommandEncoder({});
        },
        () => {
            renderBundleEncoder = device.createRenderBundleEncoder({colorFormats: [format]});
        },
        () => {
            querySet = device.createQuerySet({type: "occlusion", count: 2});
        },
        () => {
            bindGroup.label = "bindGroup";
            bindGroup.label;
        },
        () => {
            bindGroupLayout.label = "bindGroupLayout";
            bindGroupLayout.label;
        },
        () => {
            externalTexture.label = "externalTexture";
            externalTexture.label;
        },
        () => {
            pipelineLayout.label = "pipelineLayout";
            pipelineLayout.label;
        },
        () => {
            sampler.label = "sampler";
            sampler.label;
        },
        () => {
            texture.label = "texture";
            texture.label;
        },
        () => {
            textureView = texture.createView({});
        },
        () => {
            textureView.label = "textureView";
            textureView.label;
        },
        () => {
            shaderModule.label = "shaderModule";
            shaderModule.label;
        },
        () => shaderModule.getCompilationInfo().catch(() => { }),
        () => {
            computePipeline.label = "computePipeline";
            computePipeline.label;
        },
        () => {
            computePipeline.getBindGroupLayout(0);
        },
        () => {
            renderPipeline.label = "renderPipeline";
            renderPipeline.label;
        },
        () => {
            renderPipeline.getBindGroupLayout(0);
        },
        () => {
            querySet.label = "querySet";
            querySet.label;
        },
        () => {
            renderBundleEncoder.label = "renderBundleEncoder";
            renderBundleEncoder.label;
        },
        () => {
            renderBundleEncoder.pushDebugGroup("debugGroup");
        },
        () => {
            renderBundleEncoder.insertDebugMarker("debugMarker");
        },
        () => {
            renderBundleEncoder.popDebugGroup();
        },
        () => {
            renderBundleEncoder.setBindGroup(0, bindGroup, [0]);
            renderBundleEncoder.setBindGroup(0, bindGroup, new Uint32Array([0]), 0, 1);
        },
        () => {
            renderBundleEncoder.setPipeline(renderPipeline);
        },
        () => {
            renderBundleEncoder.setIndexBuffer(buffer, "uint16", 0, 4);
        },
        () => {
            renderBundleEncoder.setVertexBuffer(0, buffer, 0, 4);
        },
        () => {
            renderBundleEncoder.draw(3, 1, 0, 0);
        },
        () => {
            renderBundleEncoder.drawIndexed(3, 1, 0, 0, 0);
        },
        () => {
            renderBundleEncoder.drawIndirect(buffer, 0);
        },
        () => {
            renderBundleEncoder.drawIndexedIndirect(buffer, 0);
        },
        () => {
            renderBundle = renderBundleEncoder.finish({});
        },
        () => {
            renderBundle.label = "renderBundle";
            renderBundle.label;
        },
        () => {
            commandEncoder.label = "commandEncoder";
            commandEncoder.label;
        },
        () => {
            commandEncoder.pushDebugGroup("debugGroup");
        },
        () => {
            commandEncoder.insertDebugMarker("debugMarker");
        },
        () => {
            commandEncoder.popDebugGroup();
        },
        () => {
            commandEncoder.copyBufferToBuffer(buffer, buffer, 4);
            commandEncoder.copyBufferToBuffer(buffer, 0, buffer, 4, 4);
        },
        () => {
            commandEncoder.copyBufferToTexture({buffer, offset: 4, bytesPerRow: 256, rowsPerImage: 1}, {texture}, {width: 1, height: 1});
        },
        () => {
            commandEncoder.copyTextureToBuffer({texture, mipLevel: 1, origin: [1, 1, 0], aspect: "depth-only"}, {buffer, bytesPerRow: 256}, [1, 1, 1]);
        },
        () => {
            commandEncoder.copyTextureToTexture({texture, origin: {x: 1, y: 1, z: 0}}, {texture, origin: [0, 0, 0]}, {width: 1, height: 1});
        },
        () => {
            commandEncoder.clearBuffer(buffer, 0, 4);
        },
        () => {
            computePassEncoder = commandEncoder.beginComputePass({
                timestampWrites: {
                    querySet,
                    beginningOfPassWriteIndex: 0,
                    endOfPassWriteIndex: 1,
                },
            });
        },
        () => {
            computePassEncoder.label = "computePassEncoder";
            computePassEncoder.label;
        },
        () => {
            computePassEncoder.pushDebugGroup("debugGroup");
        },
        () => {
            computePassEncoder.insertDebugMarker("debugMarker");
        },
        () => {
            computePassEncoder.popDebugGroup();
        },
        () => {
            computePassEncoder.setBindGroup(0, bindGroup, [0]);
            computePassEncoder.setBindGroup(0, bindGroup, new Uint32Array([0]), 0, 1);
        },
        () => {
            computePassEncoder.setPipeline(computePipeline);
        },
        () => {
            computePassEncoder.dispatchWorkgroups(1, 2, 3);
        },
        () => {
            computePassEncoder.dispatchWorkgroupsIndirect(buffer, 0);
        },
        () => {
            computePassEncoder.end();
        },
        () => {
            renderPassEncoder = commandEncoder.beginRenderPass(createRenderPassDescriptor());
        },
        () => {
            renderPassEncoder.label = "renderPassEncoder";
            renderPassEncoder.label;
        },
        () => {
            renderPassEncoder.pushDebugGroup("debugGroup");
        },
        () => {
            renderPassEncoder.insertDebugMarker("debugMarker");
        },
        () => {
            renderPassEncoder.popDebugGroup();
        },
        () => {
            renderPassEncoder.setBindGroup(0, bindGroup, [0]);
            renderPassEncoder.setBindGroup(0, bindGroup, new Uint32Array([0]), 0, 1);
        },
        () => {
            renderPassEncoder.setPipeline(renderPipeline);
        },
        () => {
            renderPassEncoder.setIndexBuffer(buffer, "uint16", 0, 4);
        },
        () => {
            renderPassEncoder.setVertexBuffer(0, buffer, 0, 4);
        },
        () => {
            renderPassEncoder.draw(3, 1, 0, 0);
        },
        () => {
            renderPassEncoder.drawIndexed(3, 1, 0, 0, 0);
        },
        () => {
            renderPassEncoder.drawIndirect(buffer, 0);
        },
        () => {
            renderPassEncoder.drawIndexedIndirect(buffer, 0);
        },
        () => {
            renderPassEncoder.setViewport(0, 0, 4, 4, 0, 1);
        },
        () => {
            renderPassEncoder.setScissorRect(0, 0, 4, 4);
        },
        () => {
            renderPassEncoder.setBlendConstant([1, 0, 0, 1]);
            renderPassEncoder.setBlendConstant({r: 0, g: 1, b: 0, a: 1});
        },
        () => {
            renderPassEncoder.setStencilReference(1);
        },
        () => {
            renderPassEncoder.beginOcclusionQuery(0);
        },
        () => {
            renderPassEncoder.endOcclusionQuery();
        },
        () => {
            renderPassEncoder.executeBundles([renderBundle]);
        },
        () => {
            renderPassEncoder.end();
        },
        () => {
            commandEncoder.resolveQuerySet(querySet, 0, 1, buffer, 0);
        },
        () => {
            commandBuffer = commandEncoder.finish({});
        },
        () => {
            commandBuffer.label = "commandBuffer";
            commandBuffer.label;
        },
        () => {
            queue = device.queue;
            queue.label = "queue";
            queue.label;
        },
        () => {
            queue.writeBuffer(buffer, 0, new Uint8Array(4), 0, 4);
            queue.writeBuffer(buffer, 0, new ArrayBuffer(4), 0, 4);
        },
        () => {
            queue.writeTexture({texture}, new Uint8Array(4), {bytesPerRow: 256}, [1, 1, 1]);
        },
        () => {
            ignoreException(() => queue.copyExternalImageToTexture(
                {source: document.querySelector("canvas"), origin: [0, 0], flipY: true},
                {
                    texture,
                    mipLevel: 1,
                    origin: {x: 0, y: 0, z: 0},
                    aspect: "stencil-only",
                    colorSpace: "srgb",
                    premultipliedAlpha: true,
                },
                {width: 1, height: 1}
            ));
        },
        () => {
            ignoreException(() => queue.copyElementImageToTexture({
                source: document.querySelector("canvas"),
                sx: 0,
                sy: 0,
                swidth: 1,
                sheight: 1,
            }, {
                destination: {texture},
                width: 1,
                height: 1,
            }));
        },
        () => {
            queue.submit([commandBuffer]);
        },
        () => queue.onSubmittedWorkDone().catch(() => { }),
        () => {
            buffer.label = "buffer";
            buffer.label;
        },
        () => buffer.mapAsync(GPUMapMode.READ, 0, 4).catch(() => { }),
        () => {
            ignoreException(() => buffer.getMappedRange(0, 4));
        },
        () => {
            buffer.unmap();
        },
        () => {
            buffer.destroy();
        },
        () => {
            texture.destroy();
        },
        () => {
            querySet.destroy();
        },
        () => {
            device.destroy();
            videoFrame.close();
        },
    ];

    let index = 0;
    async function executeFrameFunction() {
        await frames[index++]();
        if (index < frames.length)
            requestAnimationFrameId = requestAnimationFrame(executeFrameFunction);
        else
            setTimeout(() => TestPage.dispatchEventToFrontend("LastFrame"), 0);
    }
    executeFrameFunction();
}

function performConsoleActions() {
    console.record(device, {name: "TEST"});

    device.label = "recorded";

    console.recordEnd(device);

    device.label;
}

async function load() {
    let adapter = await navigator.gpu.requestAdapter();
    device = await adapter.requestDevice();

    context = document.querySelector("canvas").getContext("webgpu");
    format = navigator.gpu.getPreferredCanvasFormat();
    context.configure({device, format});

    videoFrame = new VideoFrame(new ArrayBuffer(16), {
        codedWidth: 2,
        codedHeight: 2,
        format: "BGRA",
        timestamp: 0,
    });

    runTest();
}
