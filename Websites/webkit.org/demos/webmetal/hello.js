let gpu;
let commandQueue;
let renderPassDescriptor;
let renderPipelineState;

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

    renderPassDescriptor = new WebMetalRenderPassDescriptor();
    // NOTE: Our API proposal has some of these values as enums, not constant numbers.
    // We haven't got around to implementing the enums yet.
    renderPassDescriptor.colorAttachments[0].loadAction = gpu.LoadActionClear;
    renderPassDescriptor.colorAttachments[0].storeAction = gpu.StoreActionStore;
    renderPassDescriptor.colorAttachments[0].clearColor = [0.35, 0.65, 0.85, 1.0];

    render();
}

function render() {

    let commandBuffer = commandQueue.createCommandBuffer();
    let drawable = gpu.nextDrawable();
    renderPassDescriptor.colorAttachments[0].texture = drawable.texture;

    let commandEncoder = commandBuffer.createRenderCommandEncoderWithDescriptor(renderPassDescriptor);
    commandEncoder.setRenderPipelineState(renderPipelineState);

    // NOTE: We didn't attach any buffers. We create the geometry in the vertex shader using
    // the vertex ID.

    // NOTE: Our API proposal uses the enum value "triangle" here. We haven't got around to implementing the enums yet.
    commandEncoder.drawPrimitives(gpu.PrimitiveTypeTriangle, 0, 3);

    commandEncoder.endEncoding();
    commandBuffer.presentDrawable(drawable);
    commandBuffer.commit();
}
