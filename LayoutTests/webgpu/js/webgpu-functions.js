async function getBasicDevice() {
    const adapter = await navigator.gpu.requestAdapter({ powerPreference: "low-power" });
    const device = await adapter.requestDevice();
    return device;
}

function drawWhiteSquareOnBlueBackgroundInSoftware(canvas) {
    const context = canvas.getContext("2d");
    context.fillStyle = "blue";
    context.fillRect(0, 0, 400, 400);
    context.fillStyle = "white";
    context.fillRect(100, 100, 200, 200);
}

function drawBlackSquareOnBlueBackgroundInSoftware(canvas) {
    const context = canvas.getContext("2d");
    context.fillStyle = "blue";
    context.fillRect(0, 0, 400, 400);
    context.fillStyle = "black";
    context.fillRect(100, 100, 200, 200);
}

function drawGreenSquareInSoftware(canvas) {
    const context = canvas.getContext('2d');
    context.fillStyle = 'rgb(0, 255, 0)';
    context.fillRect(0, 0, canvas.width, canvas.height);
}

function drawGreenAndBlueCheckerboardInSoftware(canvas) {
    const context = canvas.getContext('2d');

    context.fillStyle = 'rgb(0, 255, 0)';
    context.fillRect(0, 0, canvas.width, canvas.height);

    const numColumns = 4;
    const numRows = 4;
    context.beginPath();
    context.fillStyle = 'rgb(0, 0, 255)';
    for (let x = 0; x < numColumns; ++x) {
        for (let y = 0; y < numRows; ++y) {
            if (x % 2 == 0 && y % 2 == 0 || x % 2 == 1 && y % 2 == 1)
                context.rect(
                    x * canvas.width / numColumns,
                    y * canvas.height / numRows,
                    canvas.width / numColumns,
                    canvas.height / numRows
                    );
        }
    }
    context.fill();
}

function createBasicSwapChain(canvas, device) {
    const context = canvas.getContext("gpu");
    return context.configureSwapChain({ device: device, format: "bgra8unorm" });
}

function createBasicDepthStateDescriptor() {
    return {
        depthWriteEnabled: true,
        depthCompare: "less"
    };
}

function createBasicDepthTexture(canvas, device) {
    const depthSize = {
        width: canvas.width,
        height: canvas.height,
        depth: 1
    };

    return device.createTexture({
        size: depthSize,
        format: "depth32float-stencil8",
        usage: GPUTextureUsage.OUTPUT_ATTACHMENT
    });
}

function createBasicPipeline(shaderModule, device, colorStates, pipelineLayout, vertexInputDescriptor, depthStateDescriptor, primitiveTopology = "triangle-strip") {
    const vertexStageDescriptor = {
        module: shaderModule,
        entryPoint: "vertex_main" 
    };

    const fragmentStageDescriptor = {
        module: shaderModule,
        entryPoint: "fragment_main"
    };

    if (!colorStates) {
        colorStates = [{ 
            format: "bgra8unorm",
            alphaBlend: {},
            colorBlend: {}
        }];
    }

    if (!vertexInputDescriptor)
        vertexInputDescriptor = { vertexBuffers: [] };

    const pipelineDescriptor = {
        vertexStage: vertexStageDescriptor,
        fragmentStage: fragmentStageDescriptor,
        primitiveTopology: primitiveTopology,
        colorStates: colorStates,
        vertexInput: vertexInputDescriptor
    };

    if (pipelineLayout)
        pipelineDescriptor.layout = pipelineLayout;

    if (depthStateDescriptor)
        pipelineDescriptor.depthStencilState = depthStateDescriptor;

    return device.createRenderPipeline(pipelineDescriptor);
}

function beginBasicRenderPass(swapChain, commandEncoder) {
    const basicAttachment = {
        attachment: swapChain.getCurrentTexture().createDefaultView(),
        loadOp: "clear",
        storeOp: "store",
        clearColor: { r: 1.0, g: 0, b: 0, a: 1.0 }
    };

    // FIXME: Flesh out the rest of GPURenderPassDescriptor. 
    return commandEncoder.beginRenderPass({ colorAttachments : [basicAttachment] });
}

function encodeBasicCommands(renderPassEncoder, renderPipeline, vertexBuffer) {
    if (vertexBuffer)
        renderPassEncoder.setVertexBuffers(0, [vertexBuffer], [0]);
    renderPassEncoder.setPipeline(renderPipeline);
    renderPassEncoder.draw(4, 1, 0, 0);
    renderPassEncoder.endPass();
}

function createBufferWithData(device, descriptor, data, offset = 0) {
    const mappedBuffer = device.createBufferMapped(descriptor);
    const dataArray = new Uint8Array(mappedBuffer[1]);
    dataArray.set(new Uint8Array(data), offset);
    mappedBuffer[0].unmap();
    return mappedBuffer[0];
}

async function mapWriteDataToBuffer(buffer, data, offset = 0) {
    const arrayBuffer = await buffer.mapWriteAsync();
    const writeArray = new Uint8Array(arrayBuffer);
    writeArray.set(new Uint8Array(data), offset);
    buffer.unmap();
}
