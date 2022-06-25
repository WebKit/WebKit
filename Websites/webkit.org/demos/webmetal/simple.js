const vertexData = new Float32Array(
[
    // float4 position, float4 color
    1, -1, 1, 1, 1, 0, 1, 1,
    -1, -1, 1, 1, 0, 0, 1, 1,
    -1, -1, -1, 1, 0, 0, 0, 1,
    1, -1, -1, 1, 1, 0, 0, 1,
    1, -1, 1, 1, 1, 0, 1, 1,
    -1, -1, -1, 1, 0, 0, 0, 1,

    1, 1, 1, 1, 1, 1, 1, 1,
    1, -1, 1, 1, 1, 0, 1, 1,
    1, -1, -1, 1, 1, 0, 0, 1,
    1, 1, -1, 1, 1, 1, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, -1, -1, 1, 1, 0, 0, 1,

    -1, 1, 1, 1, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, -1, 1, 1, 1, 0, 1,
    -1, 1, -1, 1, 0, 1, 0, 1,
    -1, 1, 1, 1, 0, 1, 1, 1,
    1, 1, -1, 1, 1, 1, 0, 1,

    -1, -1, 1, 1, 0, 0, 1, 1,
    -1, 1, 1, 1, 0, 1, 1, 1,
    -1, 1, -1, 1, 0, 1, 0, 1,
    -1, -1, -1, 1, 0, 0, 0, 1,
    -1, -1, 1, 1, 0, 0, 1, 1,
    -1, 1, -1, 1, 0, 1, 0, 1,

    1, 1, 1, 1, 1, 1, 1, 1,
    -1, 1, 1, 1, 0, 1, 1, 1,
    -1, -1, 1, 1, 0, 0, 1, 1,
    -1, -1, 1, 1, 0, 0, 1, 1,
    1, -1, 1, 1, 1, 0, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,

    1, -1, -1, 1, 1, 0, 0, 1,
    -1, -1, -1, 1, 0, 0, 0, 1,
    -1, 1, -1, 1, 0, 1, 0, 1,
    1, 1, -1, 1, 1, 1, 0, 1,
    1, -1, -1, 1, 1, 0, 0, 1,
    -1, 1, -1, 1, 0, 1, 0, 1,
]);

const NumActiveUniformBuffers = 3;

let gpu;
let commandQueue;
let renderPassDescriptor;
let renderPipelineState;
let depthStencilState;
let projectionMatrix = mat4.create();

let uniformBuffers = new Array(NumActiveUniformBuffers);
var currentUniformBufferIndex = 0;

window.addEventListener("load", init, false);

function init() {

    if (!checkForWebMetal()) {
        return;
    }

    let canvas = document.querySelector("canvas");
    let canvasSize = canvas.getBoundingClientRect();
    canvas.width = canvasSize.width;
    canvas.height = canvasSize.height;

    let aspect = Math.abs(canvasSize.width / canvasSize.height);
    mat4.perspective(projectionMatrix, (2 * Math.PI) / 5, aspect, 1, 100.0);

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
    pipelineDescriptor.depthAttachmentPixelFormat = gpu.PixelFormatDepth32Float;

    renderPipelineState = gpu.createRenderPipelineState(pipelineDescriptor);

    let depthStencilDescriptor = new WebMetalDepthStencilDescriptor();
    depthStencilDescriptor.depthWriteEnabled = true;
    depthStencilDescriptor.depthCompareFunction = "less";

    depthStencilState = gpu.createDepthStencilState(depthStencilDescriptor);

    for (let i = 0; i < NumActiveUniformBuffers; i++) {
        uniformBuffers[i] = gpu.createBuffer(new Float32Array(16));
    }

    let depthTextureDescriptor = new WebMetalTextureDescriptor(gpu.PixelFormatDepth32Float, canvasSize.width, canvasSize.height, false);
    // NOTE: Our API proposal has some of these values as enums, not constant numbers.
    // We haven't got around to implementing the enums yet.
    depthTextureDescriptor.textureType = gpu.TextureType2D;
    depthTextureDescriptor.sampleCount = 1;
    depthTextureDescriptor.usage = gpu.TextureUsageUnknown;
    depthTextureDescriptor.storageMode = gpu.StorageModePrivate;

    let depthTexture = gpu.createTexture(depthTextureDescriptor);

    renderPassDescriptor = new WebMetalRenderPassDescriptor();
    // NOTE: Our API proposal has some of these values as enums, not constant numbers.
    // We haven't got around to implementing the enums yet.
    renderPassDescriptor.colorAttachments[0].loadAction = gpu.LoadActionClear;
    renderPassDescriptor.colorAttachments[0].storeAction = gpu.StoreActionStore;
    renderPassDescriptor.colorAttachments[0].clearColor = [0.35, 0.65, 0.85, 1.0];
    renderPassDescriptor.depthAttachment.loadAction = gpu.LoadActionClear;
    renderPassDescriptor.depthAttachment.storeAction = gpu.StoreActionDontCare;
    renderPassDescriptor.depthAttachment.clearDepth = 1.0;
    renderPassDescriptor.depthAttachment.texture = depthTexture;

    vertexBuffer = gpu.createBuffer(vertexData);
    render();
}

function render() {

    updateUniformData(currentUniformBufferIndex);

    let commandBuffer = commandQueue.createCommandBuffer();

    let drawable = gpu.nextDrawable();

    renderPassDescriptor.colorAttachments[0].texture = drawable.texture;

    let commandEncoder = commandBuffer.createRenderCommandEncoderWithDescriptor(renderPassDescriptor);
    commandEncoder.setDepthStencilState(depthStencilState);
    commandEncoder.setRenderPipelineState(renderPipelineState);
    commandEncoder.setVertexBuffer(vertexBuffer, 0, 0);
    commandEncoder.setVertexBuffer(uniformBuffers[currentUniformBufferIndex], 0, 1);

    // NOTE: Our API proposal uses the enum value "triangle" here. We haven't got around to implementing the enums yet.
    commandEncoder.drawPrimitives(gpu.PrimitiveTypeTriangle, 0, 36);

    commandEncoder.endEncoding();
    commandBuffer.presentDrawable(drawable);
    commandBuffer.commit();

    currentUniformBufferIndex = (currentUniformBufferIndex + 1) % NumActiveUniformBuffers;
    requestAnimationFrame(render);
}

function updateUniformData(index) {

    let viewMatrix = mat4.create();
    mat4.translate(viewMatrix, viewMatrix, vec3.fromValues(0, 0, -5));
    let now = Date.now() / 1000;
    mat4.rotate(viewMatrix, viewMatrix, 1, vec3.fromValues(Math.sin(now), Math.cos(now), 0));

    let modelViewProjectionMatrix = mat4.create();
    mat4.multiply(modelViewProjectionMatrix, projectionMatrix, viewMatrix);

    let uniform = new Float32Array(uniformBuffers[index].contents);
    for (let i = 0; i < 16; i++) {
        uniform[i] = modelViewProjectionMatrix[i];
    }
}
