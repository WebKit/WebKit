const threadsPerThreadgroup = 32;

const sourceBufferBindingNum = 0;
const outputBufferBindingNum = 1;
const uniformsBufferBindingNum = 2;

// Enough space to store 1 radius and 33 weights.
const maxUniformsSize = (32 + 2) * Float32Array.BYTES_PER_ELEMENT;

let image, context2d, device;

const width = 600;

async function init() {
    if (!navigator.gpu) {
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
    let originalArrayBuffer;
    [originalBuffer, originalArrayBuffer] = device.createBufferMapped({ size: imageSize, usage: GPUBufferUsage.STORAGE });
    const imageWriteArray = new Uint8ClampedArray(originalArrayBuffer);
    imageWriteArray.set(originalData.data);
    originalBuffer.unmap();

    storageBuffer = device.createBuffer({ size: imageSize, usage: GPUBufferUsage.STORAGE });
    resultsBuffer = device.createBuffer({ size: imageSize, usage: GPUBufferUsage.STORAGE | GPUBufferUsage.MAP_READ });
    uniformsBuffer = device.createBuffer({ size: maxUniformsSize, usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.MAP_WRITE });

    // Bind buffers to kernel   
    const bindGroupLayout = device.createBindGroupLayout({
        bindings: [{
            binding: sourceBufferBindingNum,
            visibility: GPUShaderStageBit.COMPUTE,
            type: "storage-buffer"
        }, {
            binding: outputBufferBindingNum,
            visibility: GPUShaderStageBit.COMPUTE,
            type: "storage-buffer"
        }, {
            binding: uniformsBufferBindingNum,
            visibility: GPUShaderStageBit.COMPUTE,
            type: "uniform-buffer"
        }]
    });

    horizontalBindGroup = device.createBindGroup({
        layout: bindGroupLayout,
        bindings: [{
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
        bindings: [{
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
        computeStage: {
            module: shaderModule,
            entryPoint: "horizontal"
        }
    });

    verticalPipeline = device.createComputePipeline({
        layout: pipelineLayout,
        computeStage: {
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
    const setUniformsPromise = setUniforms(radius);
    const uniformsMappingPromise = uniformsBuffer.mapWriteAsync();

    const [uniforms, uniformsArrayBuffer] = await Promise.all([setUniformsPromise, uniformsMappingPromise]);

    const uniformsWriteArray = new Float32Array(uniformsArrayBuffer);
    uniformsWriteArray.set(uniforms);
    uniformsBuffer.unmap();

    // Run horizontal pass first
    const commandEncoder = device.createCommandEncoder();
    const passEncoder = commandEncoder.beginComputePass();
    passEncoder.setBindGroup(0, horizontalBindGroup);
    passEncoder.setPipeline(horizontalPipeline);
    const numXGroups = Math.ceil(image.width / threadsPerThreadgroup);
    passEncoder.dispatch(numXGroups, image.height, 1);
    passEncoder.endPass();

    // Run vertical pass
    const verticalPassEncoder = commandEncoder.beginComputePass();
    verticalPassEncoder.setBindGroup(0, verticalBindGroup);
    verticalPassEncoder.setPipeline(verticalPipeline);
    const numYGroups = Math.ceil(image.height / threadsPerThreadgroup);
    verticalPassEncoder.dispatch(image.width, numYGroups, 1);
    verticalPassEncoder.endPass();

    device.getQueue().submit([commandEncoder.finish()]);

    // Draw resultsBuffer as imageData back into context2d
    const resultArrayBuffer = await resultsBuffer.mapReadAsync();
    const resultArray = new Uint8ClampedArray(resultArrayBuffer);
    context2d.putImageData(new ImageData(resultArray, image.width, image.height), 0, 0);
    resultsBuffer.unmap();
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

    uniforms = [radius];
    let weightSum = 0;

    for (let i = 0; i <= radius; ++i) {
        const weight = Math.exp(-i * i / twoSigma2);
        uniforms.push(weight);
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
uint getR(uint rgba)
{
    return rgba & ${byteMask};
}

uint getG(uint rgba)
{
    return (rgba >> 8) & ${byteMask};
}

uint getB(uint rgba)
{
    return (rgba >> 16) & ${byteMask};
}

uint getA(uint rgba)
{
    return (rgba >> 24) & ${byteMask};
}

uint makeRGBA(uint r, uint g, uint b, uint a)
{
    return r + (g << 8) + (b << 16) + (a << 24);
}

void accumulateChannels(thread uint[] channels, uint startColor, float weight)
{
    channels[0] += uint(float(getR(startColor)) * weight);
    channels[1] += uint(float(getG(startColor)) * weight);
    channels[2] += uint(float(getB(startColor)) * weight);
    channels[3] += uint(float(getA(startColor)) * weight);

    // Compensate for brightness-adjusted weights.
    if (channels[0] > 255)
        channels[0] = 255;

    if (channels[1] > 255)
        channels[1] = 255;

    if (channels[2] > 255)
        channels[2] = 255;

    if (channels[3] > 255)
        channels[3] = 255;
}

uint horizontallyOffsetIndex(uint index, int offset, int rowStart, int rowEnd)
{
    int offsetIndex = int(index) + offset;

    if (offsetIndex < rowStart || offsetIndex >= rowEnd)
        return index;
    
    return uint(offsetIndex);
}

uint verticallyOffsetIndex(uint index, int offset, uint length)
{
    int realOffset = offset * ${image.width};
    int offsetIndex = int(index) + realOffset;

    if (offsetIndex < 0 || offsetIndex >= int(length))
        return index;
    
    return uint(offsetIndex);
}

[numthreads(${threadsPerThreadgroup}, 1, 1)]
compute void horizontal(constant uint[] source : register(u${sourceBufferBindingNum}),
                        device uint[] output : register(u${outputBufferBindingNum}),
                        constant float[] uniforms : register(b${uniformsBufferBindingNum}),
                        float3 dispatchThreadID : SV_DispatchThreadID)
{
    int radius = int(uniforms[0]);
    int rowStart = ${image.width} * int(dispatchThreadID.y);
    int rowEnd = ${image.width} * (1 + int(dispatchThreadID.y));
    uint globalIndex = uint(rowStart) + uint(dispatchThreadID.x);

    uint[4] channels;

    for (int i = -radius; i <= radius; ++i) {
        uint startColor = source[horizontallyOffsetIndex(globalIndex, i, rowStart, rowEnd)];
        float weight = uniforms[uint(abs(i) + 1)];
        accumulateChannels(@channels, startColor, weight);
    }

    output[globalIndex] = makeRGBA(channels[0], channels[1], channels[2], channels[3]);
}

[numthreads(1, ${threadsPerThreadgroup}, 1)]
compute void vertical(constant uint[] source : register(u${sourceBufferBindingNum}),
                        device uint[] output : register(u${outputBufferBindingNum}),
                        constant float[] uniforms : register(b${uniformsBufferBindingNum}),
                        float3 dispatchThreadID : SV_DispatchThreadID)
{
    int radius = int(uniforms[0]);
    uint globalIndex = uint(dispatchThreadID.x) * ${image.height} + uint(dispatchThreadID.y);

    uint[4] channels;

    for (int i = -radius; i <= radius; ++i) {
        uint startColor = source[verticallyOffsetIndex(globalIndex, i, source.length)];
        float weight = uniforms[uint(abs(i) + 1)];
        accumulateChannels(@channels, startColor, weight);
    }

    output[globalIndex] = makeRGBA(channels[0], channels[1], channels[2], channels[3]);
}
`;
}