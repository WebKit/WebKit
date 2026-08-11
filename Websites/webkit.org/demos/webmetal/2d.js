class Uniform {
    constructor(float32Array) {
        if (float32Array && float32Array.length != 64) {
            console.log("Incorrect backing store for Uniform");
            return;
        }

        this.array = float32Array || new Float32Array(64);
    }
    // Layout is
    // 0-1 = resolution (float2)
    // 2 = time (float)
    get buffer() {
        return this.array;
    }
    get resolution() {
        return this.array.subarray(0, 2);
    }
    get time() {
        return this.array[2];
    }
    set resolution(value) {
        this.array[0] = value[0];
        this.array[1] = value[1];
    }
    set time(value) {
        this.array[2] = value;
    }
    copyValuesTo(buffer) {
        var bufferData = new Float32Array(buffer.contents);
        for (let i = 0; i < 3; i++) {
            bufferData[i] = this.array[i];
        }
    }
}

const vertexData = new Float32Array(
[
    -1, -1, 0, 1,
    1, -1, 0, 1,
    1, 1, 0, 1,
    -1, -1, 0, 1,
    -1, 1, 0, 1,
    1, 1, 0, 1,
]);

let gpu;
let commandQueue;
let renderPassDescriptor;
let renderPipelineState;

const NumActiveUniformBuffers = 3;
let uniforms = new Array(NumActiveUniformBuffers);
let uniformBuffers = new Array(NumActiveUniformBuffers);
let currentUniformBufferIndex = 0;

window.addEventListener("load", init, false);

function init() {

    if (!checkForWebMetal()) {
        return;
    }

    let canvas = document.querySelector("canvas");
    let canvasSize = canvas.getBoundingClientRect();
    canvas.width = canvasSize.width;
    canvas.height = canvasSize.height;

    gpu = canvas.getContext("webmetal");
    commandQueue = gpu.createCommandQueue();

    let library = gpu.createLibrary(document.getElementById("library").text);
    let vertexFunction = library.functionWithName("vertex_main");
    let fragmentFunction = library.functionWithName("fragment_main");

    if (!library || !fragmentFunction || !vertexFunction) {
        return;
    }

    let pipelineDescriptor = new WebMetalRenderPipelineDescriptor();
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    // NOTE: Our API proposal has these values as enums, not constant numbers.
    // We haven't got around to implementing the enums yet.
    pipelineDescriptor.colorAttachments[0].pixelFormat = gpu.PixelFormatBGRA8Unorm;

    renderPipelineState = gpu.createRenderPipelineState(pipelineDescriptor);

    for (let i = 0; i < NumActiveUniformBuffers; i++) {
        let uniform = new Uniform();
        uniform.resolution = new Float32Array([canvasSize.width, canvasSize.height]);
        uniforms[i] = uniform;
        uniformBuffers[i] = gpu.createBuffer(uniform.buffer);
    }

    renderPassDescriptor = new WebMetalRenderPassDescriptor();
    // NOTE: Our API proposal has some of these values as enums, not constant numbers.
    // We haven't got around to implementing the enums yet.
    renderPassDescriptor.colorAttachments[0].loadAction = gpu.LoadActionClear;
    renderPassDescriptor.colorAttachments[0].storeAction = gpu.StoreActionStore;
    renderPassDescriptor.colorAttachments[0].clearColor = [0.35, 0.65, 0.85, 1.0];

    vertexBuffer = gpu.createBuffer(vertexData);
    render();
}

function render() {

    updateUniformData(currentUniformBufferIndex);

    let commandBuffer = commandQueue.createCommandBuffer();

    let drawable = gpu.nextDrawable();

    renderPassDescriptor.colorAttachments[0].texture = drawable.texture;

    let commandEncoder = commandBuffer.createRenderCommandEncoderWithDescriptor(renderPassDescriptor);
    commandEncoder.setRenderPipelineState(renderPipelineState);
    commandEncoder.setVertexBuffer(vertexBuffer, 0, 0);
    commandEncoder.setVertexBuffer(uniformBuffers[currentUniformBufferIndex], 0, 1);
    commandEncoder.setFragmentBuffer(uniformBuffers[currentUniformBufferIndex], 0, 0);

    // NOTE: Our API proposal uses the enum value "triangle" here. We haven't got around to implementing the enums yet.
    commandEncoder.drawPrimitives(gpu.PrimitiveTypeTriangle, 0, 6);

    commandEncoder.endEncoding();
    commandBuffer.presentDrawable(drawable);
    commandBuffer.commit();

    currentUniformBufferIndex = (currentUniformBufferIndex + 1) % NumActiveUniformBuffers;
    requestAnimationFrame(render);
}

function updateUniformData(index) {
    var now = Date.now() % 100000 / 500;
    let uniform = uniforms[index];
    uniform.time = now;

    uniform.copyValuesTo(uniformBuffers[index]);

}
