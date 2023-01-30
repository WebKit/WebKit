// Adapted from: https://github.com/gpuweb/gpuweb/blob/4048adbfb3b952f71881ee3079dade37569dff48/samples/hello-cube.html
const positionAttributeNum  = 0;
const colorAttributeNum     = 1;
const transformBindingNum   = 0;
const bindGroupIndex        = 0;
const colorLocation         = 0;

// FIXME: we should allow semicolons after struct declarations
const shader = `
struct FragmentData {
    @builtin(position) position: vec4<f32>,
    @location(${colorLocation}) color: vec4<f32>
}

struct Uniforms {
    modelViewProjectionMatrix: mat4x4<f32>
}

@group(${bindGroupIndex}) @binding(${transformBindingNum}) var<uniform> uniforms: Uniforms;

@vertex
fn vertex_main(
    @builtin(vertex_index) VertexIndex : u32,
    @location(${positionAttributeNum}) position: vec4<f32>,
    @location(${colorAttributeNum}) color: vec4<f32>
) -> FragmentData {
    var outData: FragmentData;
    outData.position = uniforms.modelViewProjectionMatrix * position;
    outData.color = color;
    return outData;
}

@fragment
fn fragment_main(data: FragmentData) -> @location(0) vec4<f32> {
    return data.color;
}
`;

let device, context, verticesBuffer, renderPipeline, renderPassDescriptor;
let transformBuffer, bindGroup;
const projectionMatrix = mat4.create();
const mappedGroups = [];

const colorOffset = 4 * 4;
const vertexSize = 4 * 8;
const verticesArray = new Float32Array([
    // float32x4 position, float32x4 color
     1, -1,  1, 1,    1, 0, 1, 1,
    -1, -1,  1, 1,    0, 0, 1, 1,
    -1, -1, -1, 1,    0, 0, 0, 1,
     1, -1, -1, 1,    1, 0, 0, 1,
     1, -1,  1, 1,    1, 0, 1, 1,
    -1, -1, -1, 1,    0, 0, 0, 1,

     1,  1,  1, 1,    1, 1, 1, 1,
     1, -1,  1, 1,    1, 0, 1, 1,
     1, -1, -1, 1,    1, 0, 0, 1,
     1,  1, -1, 1,    1, 1, 0, 1,
     1,  1,  1, 1,    1, 1, 1, 1,
     1, -1, -1, 1,    1, 0, 0, 1,

    -1,  1,  1, 1,    0, 1, 1, 1,
     1,  1,  1, 1,    1, 1, 1, 1,
     1,  1, -1, 1,    1, 1, 0, 1,
    -1,  1, -1, 1,    0, 1, 0, 1,
    -1,  1,  1, 1,    0, 1, 1, 1,
     1,  1, -1, 1,    1, 1, 0, 1,

    -1, -1,  1, 1,    0, 0, 1, 1,
    -1,  1,  1, 1,    0, 1, 1, 1,
    -1,  1, -1, 1,    0, 1, 0, 1,
    -1, -1, -1, 1,    0, 0, 0, 1,
    -1, -1,  1, 1,    0, 0, 1, 1,
    -1,  1, -1, 1,    0, 1, 0, 1,

     1,  1,  1, 1,    1, 1, 1, 1,
    -1,  1,  1, 1,    0, 1, 1, 1,
    -1, -1,  1, 1,    0, 0, 1, 1,
    -1, -1,  1, 1,    0, 0, 1, 1,
     1, -1,  1, 1,    1, 0, 1, 1,
     1,  1,  1, 1,    1, 1, 1, 1,

     1, -1, -1, 1,    1, 0, 0, 1,
    -1, -1, -1, 1,    0, 0, 0, 1,
    -1,  1, -1, 1,    0, 1, 0, 1,
     1,  1, -1, 1,    1, 1, 0, 1,
     1, -1, -1, 1,    1, 0, 0, 1,
    -1,  1, -1, 1,    0, 1, 0, 1,
]);

async function init() {
    if (!navigator.gpu || GPUBufferUsage.COPY_SRC === undefined) {
        document.body.className = 'error';
        return;
    }

    const adapter = await navigator.gpu.requestAdapter();
    device = await adapter.requestDevice();

    // Canvas

    const canvas = document.querySelector('canvas');
    const canvasSize = canvas.getBoundingClientRect();
    canvas.width = canvasSize.width;
    canvas.height = canvasSize.height;

    const aspect = Math.abs(canvas.width / canvas.height);
    mat4.perspective(projectionMatrix, (2 * Math.PI) / 5, aspect, 1, 100.0);

    context = canvas.getContext('webgpu');
    const canvasFormat = "bgra8unorm";

    const contextConfiguration = {
        device: device,
        format: canvasFormat,
        alphaMode: 'opaque',
    };
    context.configure(contextConfiguration);

    // Bind Group Layout + Pipeline Layout

    const transformBufferBindGroupLayoutEntry = {
        binding: transformBindingNum, // @group(0) @binding(0)
        visibility: GPUShaderStage.VERTEX,
        buffer: { type: "uniform" },
    };
    const bindGroupLayoutDescriptor = { entries: [transformBufferBindGroupLayoutEntry] };
    const bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutDescriptor);

    const pipelineLayoutDescriptor = { bindGroupLayouts: [bindGroupLayout] };
    const pipelineLayout = device.createPipelineLayout(pipelineLayoutDescriptor);

    // Shader Module

    // FIXME: we should not require hints to compile with WGSL
    const shaderModuleDescriptor = { code: shader, hints: { vertex_main: { layout: pipelineLayout } } };
    const shaderModule = device.createShaderModule(shaderModuleDescriptor);

    // Vertex Buffer

    const verticesBufferDescriptor = {
        size: verticesArray.byteLength,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: true,
    };
    verticesBuffer = device.createBuffer(verticesBufferDescriptor)
    const verticesArrayBuffer = verticesBuffer.getMappedRange();

    const verticesWriteArray = new Float32Array(verticesArrayBuffer);
    verticesWriteArray.set(verticesArray);
    verticesBuffer.unmap();

    // Render Pipeline

    // Render Pipeline > Vertex Input
    const positionAttributeState = {
        format: "float32x4",
        offset: 0,
        shaderLocation: positionAttributeNum,  // @attribute(0)
    };
    const colorAttributeState = {
        format: "float32x4",
        offset: colorOffset,
        shaderLocation: colorAttributeNum,  // @attribute(1)
    }
    const vertexBufferState = {
        arrayStride: vertexSize,
        stepMode: "vertex",
        attributes: [positionAttributeState, colorAttributeState],
    };

    // Render Pipeline > Depth/Stencil State
    const depthFormat = "depth24plus";
    const depthStateDescriptor = {
        format: depthFormat,
        depthWriteEnabled: true,
        depthCompare: "less"
    };

    const colorTargetState = {
        format: canvasFormat,
        blend: {
            alpha: {
                srcFactor: "src-alpha",
                dstFactor: "one-minus-src-alpha",
                operation: "add"
            },
            color: {
                srcFactor: "src-alpha",
                dstFactor: "one-minus-src-alpha",
                operation: "add"
            },
        },
        writeMask: GPUColorWrite.ALL,
    };
    const renderPipelineDescriptor = {
        layout: pipelineLayout,
        vertex: {
            buffers: [vertexBufferState],
            module: shaderModule,
            entryPoint: "vertex_main"
        },
        depthStencil: depthStateDescriptor,
        fragment: {
            module: shaderModule,
            entryPoint: "fragment_main",
            targets: [colorTargetState],
        },
        // FIXME: we should not need to specify this attribute, as it's the default value
        primitive: {topology: "triangle-list" },
    };
    renderPipeline = device.createRenderPipeline(renderPipelineDescriptor);

    // Render Pass Descriptor

    const colorAttachment = {
        // attachment is acquired in render loop.
        clearValue: { r: 0.5, g: 1.0, b: 1.0, a: 1.0 }, // GPUColor
        loadOp: "clear",
        storeOp: "store",
    };

    // Depth stencil texture

    // GPUExtent3D
    const depthSize = {
        width: canvas.width,
        height: canvas.height,
        depthOrArrayLayers: 1
    };

    const depthTextureDescriptor = {
        size: depthSize,
        arrayLayerCount: 1,
        mipLevelCount: 1,
        sampleCount: 1,
        dimension: "2d",
        format: depthFormat,
        usage: GPUTextureUsage.RENDER_ATTACHMENT
    };

    const depthTexture = device.createTexture(depthTextureDescriptor);

    // GPURenderPassDepthStencilAttachmentDescriptor
    const depthAttachment = {
        view: depthTexture.createView(),
        depthClearValue: 1.0,
        depthLoadOp: "clear",
        depthStoreOp: "store",
        stencilClearValue: 0,
        stencilLoadOp: "load",
        stencilStoreOp: "store",
    };

    renderPassDescriptor = {
        colorAttachments: [colorAttachment],
        depthStencilAttachment: depthAttachment
    };

    // Transform Buffers and Bindings

    const transformSize = 4 * 16;
    const transformBufferDescriptor = {
        size: transformSize,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    };
    transformBuffer = device.createBuffer(transformBufferDescriptor)

    const transformBufferBinding = {
        buffer: transformBuffer,
        offset: 0,
        size: transformSize
    };
    const transformBufferBindGroupEntry = {
        binding: transformBindingNum,
        resource: transformBufferBinding
    };
    const bindGroupDescriptor = {
        layout: bindGroupLayout,
        entries: [transformBufferBindGroupEntry]
    };
    bindGroup = device.createBindGroup(bindGroupDescriptor);

    render();
}

function render() {
    updateTransformArray();

    const commandEncoder = device.createCommandEncoder();
    renderPassDescriptor.colorAttachments[0].view = context.getCurrentTexture().createView();
    const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);

    // Encode drawing commands

    passEncoder.setPipeline(renderPipeline);
    // Vertex attributes
    passEncoder.setVertexBuffer(0, verticesBuffer);
    // Bind groups
    passEncoder.setBindGroup(bindGroupIndex, bindGroup);
    // 36 vertices, 1 instance, 0th vertex, 0th instance.
    passEncoder.draw(36, 1, 0, 0);
    passEncoder.end();

    device.queue.submit([commandEncoder.finish()]);

    requestAnimationFrame(render);
}

function updateTransformArray() {
    const viewMatrix = mat4.create();
    mat4.translate(viewMatrix, viewMatrix, vec3.fromValues(0, 0, -5));
    const now = Date.now() / 1000;
    mat4.rotate(viewMatrix, viewMatrix, 1, vec3.fromValues(Math.sin(now), Math.cos(now), 0));
    const modelViewProjectionMatrix = mat4.create();
    mat4.multiply(modelViewProjectionMatrix, projectionMatrix, viewMatrix);
    // FIXME: we should not require the 4th argument here
    device.queue.writeBuffer(transformBuffer, 0, modelViewProjectionMatrix, 0);
}

window.addEventListener("DOMContentLoaded", init);
