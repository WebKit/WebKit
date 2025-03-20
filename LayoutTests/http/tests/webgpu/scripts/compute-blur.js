const threadsPerThreadgroup = 32;

const sourceBufferBindingNum = 0;
const outputBufferBindingNum = 1;
const uniformsBufferBindingNum = 2;

// Enough space to store 1 radius and 33 weights.
const maxUniformsSize = (32 + 2) * Float32Array.BYTES_PER_ELEMENT;

let image, context2d, device;

const width = 600;

async function init() {
    if (!navigator.gpu || GPUBufferUsage.COPY_SRC === undefined) {
        document.body.className = "error";
        return;
    }

    const slider = document.querySelector("input");
    const canvas = document.querySelector("canvas");
    context2d = canvas.getContext("2d");

    const adapter = await navigator.gpu.requestAdapter();
    device = await adapter.requestDevice();
    image = await loadImage(canvas);

    setUpCompute();

    let busy = false;
    let inputQueue = [];
    slider.oninput = async () => {
        inputQueue.push(slider.value);
        
        if (busy)
            return;

        busy = true;
        while (inputQueue.length != 0)
            await computeBlur(inputQueue.shift());
        busy = false;
    };
}

async function loadImage(canvas) {
    /* Image */
    const image = new Image();
    const imageLoadPromise = new Promise(resolve => { 
        image.onload = () => resolve(); 
        image.src = "resources/safari-alpha.png"
    });
    await Promise.resolve(imageLoadPromise);

    canvas.height = width;
    canvas.width = width;

    context2d.drawImage(image, 0, 0, width, width);

    return image;
}

let originalData, imageSize;
let originalBuffer, storageBuffer, resultsBuffer, uniformsBuffer;
let horizontalBindGroup, verticalBindGroup, horizontalPipeline, verticalPipeline;

function setUpCompute() {
    originalData = context2d.getImageData(0, 0, image.width, image.height);
    imageSize = originalData.data.length;

    // Buffer creation
    originalBuffer = device.createBuffer({
        size: imageSize,
        usage: GPUBufferUsage.STORAGE,
        mappedAtCreation: true,
    });
    const imageWriteArray = new Uint8ClampedArray(originalBuffer.getMappedRange());
    imageWriteArray.set(originalData.data);
    originalBuffer.unmap();

    storageBuffer = device.createBuffer({ size: imageSize, usage: GPUBufferUsage.STORAGE });
    resultsBuffer = device.createBuffer({ size: imageSize, usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC });
    outputBuffer = device.createBuffer({ size: imageSize, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });
    uniformsBuffer = device.createBuffer({ size: maxUniformsSize, usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST });

    // Bind buffers to kernel   
    const bindGroupLayout = device.createBindGroupLayout({
        entries: [{
            binding: sourceBufferBindingNum,
            visibility: GPUShaderStage.COMPUTE,
            buffer: { type: "storage" },
        }, {
            binding: outputBufferBindingNum,
            visibility: GPUShaderStage.COMPUTE,
            buffer: { type: "storage" },
        }, {
            binding: uniformsBufferBindingNum,
            visibility: GPUShaderStage.COMPUTE,
            buffer: { type: "storage" },
        }]
    });

    horizontalBindGroup = device.createBindGroup({
        layout: bindGroupLayout,
        entries: [{
            binding: sourceBufferBindingNum,
            resource: {
                buffer: originalBuffer,
                size: imageSize
            }
        }, {
            binding: outputBufferBindingNum,
            resource: {
                buffer: storageBuffer,
                size: imageSize
            }
        }, {
            binding: uniformsBufferBindingNum,
            resource: {
                buffer: uniformsBuffer,
                size: maxUniformsSize
            }
        }]
    });

    verticalBindGroup = device.createBindGroup({
        layout: bindGroupLayout,
        entries: [{
            binding: sourceBufferBindingNum,
            resource: {
                buffer: storageBuffer,
                size: imageSize
            }
        }, {
            binding: outputBufferBindingNum,
            resource: {
                buffer: resultsBuffer,
                size: imageSize
            }
        }, {
            binding: uniformsBufferBindingNum,
            resource: {
                buffer: uniformsBuffer,
                size: maxUniformsSize
            }
        }]
    });

    // Set up pipelines
    const pipelineLayout = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

    const shaderModule = device.createShaderModule({ code: createShaderCode(image), isWHLSL: true });

    horizontalPipeline = device.createComputePipeline({ 
        layout: pipelineLayout, 
        compute: {
            module: shaderModule,
            entryPoint: "horizontal"
        }
    });

    verticalPipeline = device.createComputePipeline({
        layout: pipelineLayout,
        compute: {
            module: shaderModule,
            entryPoint: "vertical"
        }
    });
}

async function computeBlur(radius) {
    if (radius == 0) {
        context2d.drawImage(image, 0, 0, width, width);
        return;
    }
    console.log(radius);
    const uniforms = await setUniforms(radius);
    device.queue.writeBuffer(uniformsBuffer, 0, uniforms);

    // Run horizontal pass first
    const commandEncoder = device.createCommandEncoder();
    const passEncoder = commandEncoder.beginComputePass();
    passEncoder.setBindGroup(0, horizontalBindGroup);
    passEncoder.setPipeline(horizontalPipeline);
    const numXGroups = Math.ceil(image.width / threadsPerThreadgroup);
    passEncoder.dispatchWorkgroups(numXGroups, image.height, 1);
    passEncoder.end();

    // Run vertical pass
    const verticalPassEncoder = commandEncoder.beginComputePass();
    verticalPassEncoder.setBindGroup(0, verticalBindGroup);
    verticalPassEncoder.setPipeline(verticalPipeline);
    const numYGroups = Math.ceil(image.height / threadsPerThreadgroup);
    verticalPassEncoder.dispatchWorkgroups(image.width, numYGroups, 1);
    verticalPassEncoder.end();

    commandEncoder.copyBufferToBuffer(resultsBuffer, 0, outputBuffer, 0, imageSize);

    device.queue.submit([commandEncoder.finish()]);

    // Draw outputBuffer as imageData back into context2d
    await outputBuffer.mapAsync(GPUMapMode.READ);
    const resultArray = new Uint8ClampedArray(outputBuffer.getMappedRange());
    context2d.putImageData(new ImageData(resultArray, image.width, image.height), 0, 0);
    outputBuffer.unmap();
}

window.addEventListener("load", init);

/* Helpers */

let uniformsCache = new Map();

async function setUniforms(radius)
{
    let uniforms = uniformsCache.get(radius);
    if (uniforms != undefined)
        return uniforms;

    const sigma = radius / 2.0;
    const twoSigma2 = 2.0 * sigma * sigma;

    uniforms = new Float32Array(34);
    uniforms[0] = radius;
    let weightSum = 0;

    for (let i = 0; i <= radius; ++i) {
        const weight = Math.exp(-i * i / twoSigma2);
        uniforms[i+1] = weight;
        weightSum += (i == 0) ? weight : weight * 2;
    }

    // Compensate for loss in brightness
    const brightnessScale =  1 - (0.1 / 32.0) * radius;
    weightSum *= brightnessScale;
    for (let i = 1; i < uniforms.length; ++i)
        uniforms[i] /= weightSum;
        
    uniformsCache.set(radius, uniforms);

    return uniforms;
}

const byteMask = (1 << 8) - 1;

function createShaderCode(image) {
    return `
fn getR(rgba: u32) -> u32
{
    return rgba & ${byteMask};
}

fn getG(rgba: u32) -> u32
{
    return (rgba >> 8) & ${byteMask};
}

fn getB(rgba: u32) -> u32
{
    return (rgba >> 16) & ${byteMask};
}

fn getA(rgba: u32) -> u32
{
    return (rgba >> 24) & ${byteMask};
}

fn makeRGBA(r: u32, g: u32, b: u32, a: u32) -> u32
{
    return r + (g << 8) + (b << 16) + (a << 24);
}

var<private> channels : array<u32, 4>;
fn accumulateChannels(startColor: u32, weight: f32)
{
    channels[0] += u32(f32(getR(startColor)) * weight);
    channels[1] += u32(f32(getG(startColor)) * weight);
    channels[2] += u32(f32(getB(startColor)) * weight);
    channels[3] += u32(f32(getA(startColor)) * weight);

    // Compensate for brightness-adjusted weights.
    if (channels[0] > 255) {
        channels[0] = 255;
    }

    if (channels[1] > 255) {
        channels[1] = 255;
    }

    if (channels[2] > 255) {
        channels[2] = 255;
    }

    if (channels[3] > 255) {
        channels[3] = 255;
    }
}

fn horizontallyOffsetIndex(index: u32, offset: i32, rowStart: i32, rowEnd: i32) -> u32
{
    let offsetIndex = i32(index) + offset;

    if (offsetIndex < rowStart || offsetIndex >= rowEnd) {
        return index;
    }
    
    return u32(offsetIndex);
}

fn verticallyOffsetIndex(index: u32, offset: i32, length: u32) -> u32
{
    let realOffset = offset * ${image.width};
    let offsetIndex = i32(index) + realOffset;

    if (offsetIndex < 0 || offsetIndex >= i32(length)) {
        return index;
    }
    
    return u32(offsetIndex);
}

@group(0) @binding(${sourceBufferBindingNum}) var<storage, read_write> source : array<u32>;
@group(0) @binding(${outputBufferBindingNum}) var<storage, read_write> output : array<u32>;
@group(0) @binding(${uniformsBufferBindingNum}) var<storage, read_write> uniforms : array<f32>;

@workgroup_size(${threadsPerThreadgroup}, 1, 1)
@compute
fn horizontal(@builtin(global_invocation_id) dispatchThreadID: vec3<u32>)
{
    let radius = i32(uniforms[0]);
    let rowStart = ${image.width} * i32(dispatchThreadID.y);
    let rowEnd = ${image.width} * (1 + i32(dispatchThreadID.y));
    let globalIndex = u32(rowStart) + u32(dispatchThreadID.x);

    var i = -radius;
    loop {
        if i > radius { break; }

        let startColor = source[horizontallyOffsetIndex(globalIndex, i, rowStart, rowEnd)];
        let weight = uniforms[u32(abs(i) + 1)];
        accumulateChannels(startColor, weight);

        i++;
    }

    output[globalIndex] = makeRGBA(channels[0], channels[1], channels[2], channels[3]);
}

@workgroup_size(1, ${threadsPerThreadgroup}, 1)
@compute
fn vertical(@builtin(global_invocation_id) dispatchThreadID: vec3<u32>)
{
    let radius = i32(uniforms[0]);
    let globalIndex = u32(dispatchThreadID.x) * ${image.height} + u32(dispatchThreadID.y);

    var i = -radius;
    loop {
        if i > radius { break; }

        let startColor = source[verticallyOffsetIndex(globalIndex, i, arrayLength(&source))];
        let weight = uniforms[u32(abs(i) + 1)];
        accumulateChannels(startColor, weight);

        i++;
    }

    output[globalIndex] = makeRGBA(channels[0], channels[1], channels[2], channels[3]);
}
`;
}
