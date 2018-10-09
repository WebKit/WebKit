class Uniform {
    constructor(float32Array) {
        if (float32Array && float32Array.length != 64) {
            console.log("Incorrect backing store for Uniform");
            return;
        }

        this.array = float32Array || new Float32Array(64);
    }
    // Layout is
    // 0-15 = model_view_projection_matrix (float4x4)
    // 16-31 = normal matrix (float4x4)
    // 32-35 = ambient color (float4)
    // 36 = multiplier (int)
    get buffer() {
        return this.array;
    }
    get mvp() {
        return this.array.subarray(0, 16);
    }
    get normal() {
        return this.array.subarray(16, 32);
    }
    get ambientColor() {
        return this.array.subarray(32, 36);
    }
    get multiplier() {
        return this.array[40];
    }
    set mvp(value) {
        this._copyMatrix(value, 0);
    }
    set normal(value) {
        this._copyMatrix(value, 16);
    }
    set ambientColor(value) {
        this.array[32] = value[0];
        this.array[33] = value[1];
        this.array[34] = value[2];
        this.array[35] = value[3];
    }
    set multiplier(value) {
        this.array[36] = value;
    }
    _copyMatrix(matrix, offset) {
        for (var i = 0; i < 16; i++) {
            this.array[offset + i] = matrix[i];
        }
    }
}

const smoothstep = (min, max, value) => {
    let x = Math.max(0, Math.min(1, (value - min) / (max - min)));
    return x * x * (3 - 2 * x);
};

const inTimeRange = (now, start, end, period) => {
    let offset = now % period;
    return offset >= start && offset <= end;
};

const timeRangeOffset = (now, start, end, period) => {
    if (!inTimeRange(now, start, end, period)) {
        return 0;
    }
    let offset = now % period;
    return Math.min(1, Math.max(0, (offset - start) / (end - start)));
};

const middlePeakTimeRangeOffset = (now, start, end, period) => {
    let offset = timeRangeOffset(now, start, end, period);
    return 1 - Math.abs(0.5 - offset) * 2;
};

const kNumActiveUniformBuffers = 3;
const kNumberOfBoxesPerAxis = 6;
const kNumberOfBoxes = kNumberOfBoxesPerAxis * kNumberOfBoxesPerAxis;
const kBoxBaseAmbientColor = [0.2, 0.2, 0.2, 1.0];
const kFOVY = 45.0 / 180 * Math.PI;
const kEye = vec3.fromValues(0.0, 2.75, -2);
const kCenter = vec3.fromValues(0.0, 1.5, 1.0);
const kUp = vec3.fromValues(0.0, 1.0, 0.0);
const kEdge = 1.5 / kNumberOfBoxesPerAxis ;

const kCubeVertexData = new Float32Array(
[
    kEdge, -kEdge, kEdge, 0.0, -1.0, 0.0,
    -kEdge, -kEdge, kEdge, 0.0, -1.0, 0.0,
    -kEdge, -kEdge, -kEdge, 0.0, -1.0, 0.0,
    kEdge, -kEdge, -kEdge, 0.0, -1.0, 0.0,
    kEdge, -kEdge, kEdge, 0.0, -1.0, 0.0,
    -kEdge, -kEdge, -kEdge, 0.0, -1.0, 0.0,

    kEdge, kEdge, kEdge, 1.0, 0.0, 0.0,
    kEdge, -kEdge, kEdge, 1.0, 0.0, 0.0,
    kEdge, -kEdge, -kEdge, 1.0, 0.0, 0.0,
    kEdge, kEdge, -kEdge, 1.0, 0.0, 0.0,
    kEdge, kEdge, kEdge, 1.0, 0.0, 0.0,
    kEdge, -kEdge, -kEdge, 1.0, 0.0, 0.0,

    -kEdge, kEdge, kEdge, 0.0, 1.0, 0.0,
    kEdge, kEdge, kEdge, 0.0, 1.0, 0.0,
    kEdge, kEdge, -kEdge, 0.0, 1.0, 0.0,
    -kEdge, kEdge, -kEdge, 0.0, 1.0, 0.0,
    -kEdge, kEdge, kEdge, 0.0, 1.0, 0.0,
    kEdge, kEdge, -kEdge, 0.0, 1.0, 0.0,

    -kEdge, -kEdge, kEdge, -1.0, 0.0, 0.0,
    -kEdge, kEdge, kEdge, -1.0, 0.0, 0.0,
    -kEdge, kEdge, -kEdge, -1.0, 0.0, 0.0,
    -kEdge, -kEdge, -kEdge, -1.0, 0.0, 0.0,
    -kEdge, -kEdge, kEdge, -1.0, 0.0, 0.0,
    -kEdge, kEdge, -kEdge, -1.0, 0.0, 0.0,

    kEdge, kEdge, kEdge, 0.0, 0.0, 1.0,
    -kEdge, kEdge, kEdge, 0.0, 0.0, 1.0,
    -kEdge, -kEdge, kEdge, 0.0, 0.0, 1.0,
    -kEdge, -kEdge, kEdge, 0.0, 0.0, 1.0,
    kEdge, -kEdge, kEdge, 0.0, 0.0, 1.0,
    kEdge, kEdge, kEdge, 0.0, 0.0, 1.0,

    kEdge, -kEdge, -kEdge, 0.0, 0.0, -1.0,
    -kEdge, -kEdge, -kEdge, 0.0, 0.0, -1.0,
    -kEdge, kEdge, -kEdge, 0.0, 0.0, -1.0,
    kEdge, kEdge, -kEdge, 0.0, 0.0, -1.0,
    kEdge, -kEdge, -kEdge, 0.0, 0.0, -1.0,
    -kEdge, kEdge, -kEdge, 0.0, 0.0, -1.0
]);

let gpu;
let commandQueue;
let renderPassDescriptor;
let renderPipelineState;
let depthStencilState;

let projectionMatrix = mat4.create();
let viewMatrix = mat4.create();

var startTime;
var elapsedTime;
let cameraRotation = 0;
let cameraAltitude = 0;

const uniformBuffers = new Array(kNumActiveUniformBuffers);
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
    mat4.perspective(projectionMatrix, kFOVY, aspect, 0.1, 100.0);
    mat4.lookAt(viewMatrix, kEye, kCenter, kUp);

    gpu = canvas.getContext("webmetal");
    commandQueue = gpu.createCommandQueue();

    let library = gpu.createLibrary(document.getElementById("library").text);
    let vertexFunction = library.functionWithName("vertex_function");
    let fragmentFunction = library.functionWithName("fragment_function");

    if (!library || !fragmentFunction || !vertexFunction) {
        return;
    }

    var pipelineDescriptor = new WebMetalRenderPipelineDescriptor();
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

    for (let i = 0; i < kNumActiveUniformBuffers; i++) {
        uniformBuffers[i] = [];
        for (let j = 0; j < kNumberOfBoxes; j++) {
            let uniform = new Uniform();
            uniform.multiplier = (j % 2) ? -1 : 1;
            uniform.ambientColor = kBoxBaseAmbientColor;
            uniformBuffers[i].push(gpu.createBuffer(uniform.buffer));
        }
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

    vertexBuffer = gpu.createBuffer(kCubeVertexData);

    startTime = Date.now();
    render();
}

function render() {

    elapsedTime = (Date.now() - startTime) / 1000;

    cameraRotation = Math.PI * 2 * (1 - timeRangeOffset(elapsedTime, 0, 11, 11));
    cameraAltitude = Math.sin(elapsedTime / 6 * Math.PI) + 1;

    let eye = vec3.create();
    vec3.add(eye, kEye, vec3.fromValues(0, cameraAltitude, 0));
    mat4.lookAt(viewMatrix, eye, kCenter, kUp);

    updateUniformData(currentUniformBufferIndex);

    let commandBuffer = commandQueue.createCommandBuffer();

    let drawable = gpu.nextDrawable();

    renderPassDescriptor.colorAttachments[0].texture = drawable.texture;

    let commandEncoder = commandBuffer.createRenderCommandEncoderWithDescriptor(renderPassDescriptor);
    commandEncoder.setDepthStencilState(depthStencilState);
    commandEncoder.setRenderPipelineState(renderPipelineState);
    commandEncoder.setVertexBuffer(vertexBuffer, 0, 0);

    for (let geometryBuffer of uniformBuffers[currentUniformBufferIndex]) {
        commandEncoder.setVertexBuffer(geometryBuffer, 0, 1);
        // NOTE: Our API proposal uses the enum value "triangle" here. We haven't got around to implementing the enums yet.
        commandEncoder.drawPrimitives(gpu.PrimitiveTypeTriangle, 0, kCubeVertexData.length);
    }

    commandEncoder.endEncoding();
    commandBuffer.presentDrawable(drawable);
    commandBuffer.commit();

    currentUniformBufferIndex = (currentUniformBufferIndex + 1) % kNumActiveUniformBuffers;
    requestAnimationFrame(render);
}

function updateUniformData(index) {

    let baseModelViewMatrix = mat4.create();
    mat4.translate(baseModelViewMatrix, baseModelViewMatrix, vec3.fromValues(0, -1 * cameraAltitude, 5));
    mat4.rotate(baseModelViewMatrix, baseModelViewMatrix, cameraRotation, vec3.fromValues(0, 1, 0));

    mat4.multiply(baseModelViewMatrix, viewMatrix, baseModelViewMatrix);

    for (let i = 0; i < kNumberOfBoxesPerAxis; i++) {
        for (let j = 0; j < kNumberOfBoxesPerAxis; j++) {

            let boxIndex = i * kNumberOfBoxesPerAxis + j;

            let modelViewMatrix = mat4.create();

            // Position the cube in X,Y.
            let translationMatrix = mat4.create();
            if (kNumberOfBoxesPerAxis > 1) {
                let step = 4 / (kNumberOfBoxesPerAxis - 1);
                mat4.translate(translationMatrix, translationMatrix, vec3.fromValues(j * step - 2, 0, i * step - 2));
            } else {
                mat4.translate(translationMatrix, translationMatrix, vec3.fromValues(0, 0, 0));
            }

            let translateElapsedOffset = elapsedTime + boxIndex * 0.6;
            if (inTimeRange(translateElapsedOffset, 10, 12, 23)) {
                let translate = smoothstep(0, 1, middlePeakTimeRangeOffset(translateElapsedOffset, 10, 12, 23)) * 0.4;
                mat4.translate(translationMatrix, translationMatrix, vec3.fromValues(0, translate, 0));
            }

            let scaleMatrix = mat4.create();
            let scaleElapsedOffset = elapsedTime + boxIndex * 0.1;
            if (inTimeRange(scaleElapsedOffset, 2, 6, 19)) {
                let scale = smoothstep(0, 1, middlePeakTimeRangeOffset(scaleElapsedOffset, 2, 6, 19)) * 0.5 + 1;
                mat4.scale(scaleMatrix, scaleMatrix, vec3.fromValues(scale, scale, scale));
            }
            mat4.multiply(modelViewMatrix, translationMatrix, scaleMatrix);

            // Rotate the cube.
            let rotationMatrix = mat4.create();
            let rotationElapsedOffset = elapsedTime + (kNumberOfBoxes - boxIndex) * 0.1;
            if (inTimeRange(rotationElapsedOffset, 3, 7, 11)) {
                var rotation = smoothstep(0, 1, timeRangeOffset(rotationElapsedOffset, 3, 7, 11)) * Math.PI * 2;
                mat4.rotate(rotationMatrix, rotationMatrix, rotation, vec3.fromValues(0.5, 1, 1));
            }
            mat4.multiply(modelViewMatrix, modelViewMatrix, rotationMatrix);

            mat4.multiply(modelViewMatrix, baseModelViewMatrix, modelViewMatrix);

            let normalMatrix = mat4.clone(modelViewMatrix);
            mat4.transpose(normalMatrix, normalMatrix);
            mat4.invert(normalMatrix, normalMatrix);

            let uniform = new Uniform(new Float32Array(uniformBuffers[index][i * kNumberOfBoxesPerAxis + j].contents));

            uniform.normal = normalMatrix;

            let modelViewProjectionMatrix = mat4.create();
            mat4.multiply(modelViewProjectionMatrix, projectionMatrix, modelViewMatrix);

            uniform.mvp = modelViewProjectionMatrix;

            let colorElapsedOffset = elapsedTime + boxIndex * 0.15;
            if (inTimeRange(colorElapsedOffset, 2, 3, 10)) {
                let redBoost = middlePeakTimeRangeOffset(colorElapsedOffset, 2, 3, 10) * 0.5;
                uniform.ambientColor[0] = Math.min(1.0, Math.max(0, kBoxBaseAmbientColor[0] + redBoost));
            } else {
                uniform.ambientColor[0] = kBoxBaseAmbientColor[0];
            }
            colorElapsedOffset = elapsedTime + (kNumberOfBoxes - boxIndex) * 0.1;
            if (inTimeRange(colorElapsedOffset, 7, 11, 21)) {
                let greenBoost = middlePeakTimeRangeOffset(colorElapsedOffset, 7, 11, 21) * 0.3;
                uniform.ambientColor[1] = Math.min(1.0, Math.max(0, kBoxBaseAmbientColor[1] + greenBoost));
            } else {
                uniform.ambientColor[1] = kBoxBaseAmbientColor[1];
            }
        }
    }
}
