if (window.internals) {
    window.internals.settings.setWebGLErrorsToConsoleEnabled(false);
}

// 2x2 red square
let image = document.createElement("img");
image.src = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAAXNSR0IArs4c6QAAABNJREFUCB1j/M/AAEQMDEwgAgQAHxcCAmtAm/sAAAAASUVORK5CYII=";

let float32Array = new Float32Array([0.1, 0.2]);
let int32Array = new Int32Array([1, 2]);

let buffer = null;
let framebuffer = null;
let uniformLocation = null;
let renderbuffer = null;
let shader = null;
let texture = null;

function load() {
    createProgram("webgl2");
    linkProgram("vertex-shader", "fragment-shader");

    context.canvas.width = 2;
    context.canvas.height = 2;

    buffer = context.createBuffer();
    framebuffer = context.createFramebuffer();
    uniformLocation = context.getUniformLocation(program, "testUniform");
    renderbuffer = context.createRenderbuffer();
    shader = context.createShader(context.VERTEX_SHADER);
    texture = context.createTexture();

    cancelActions();

    runTest();
}

function ignoreException(func){
    try {
        func();
    } catch (e) { }
}

let requestAnimationFrameId = NaN;

function cancelActions() {
    cancelAnimationFrame(requestAnimationFrameId);
    requestAnimationFrameId = NaN;

    context.clearColor(0.0, 0.0, 0.0, 0.0);
    context.clear(context.COLOR_BUFFER_BIT);
}

function performActions() {
    let frames = [
        () => {
            context.activeTexture(1);
        },
        () => {
            context.attachShader(program, shader);
        },
        () => {
            context.bindAttribLocation(program, 1, "test");
        },
        () => {
            context.bindBuffer(1, buffer);
        },
        () => {
            context.bindFramebuffer(1, framebuffer);
        },
        () => {
            context.bindRenderbuffer(1, renderbuffer);
        },
        () => {
            context.bindTexture(1, texture);
        },
        () => {
            context.blendColor(1, 2, 3, 4);
        },
        () => {
            context.blendEquation(1);
        },
        () => {
            context.blendEquationSeparate(1, 2);
        },
        () => {
            context.blendFunc(1, 2);
        },
        () => {
            context.blendFuncSeparate(1, 2, 3, 4);
        },
        () => {
            context.bufferData(1, float32Array, 2);
            context.bufferData(3, 4, 5);
        },
        () => {
            context.bufferSubData(1, 2, float32Array);
        },
        () => {
            context.checkFramebufferStatus(1);
        },
        () => {
            context.clear(1);
        },
        () => {
            context.clearColor(1, 2, 3, 4);
        },
        () => {
            context.clearDepth(1);
        },
        () => {
            context.clearStencil(1);
        },
        () => {
            context.colorMask(true, false, true, false);
        },
        () => {
            context.compileShader(shader);
        },
        () => {
            context.compressedTexSubImage2D(1, 2, 3, 4, 5, 6, 7, float32Array);
        },
        () => {
            context.copyTexImage2D(1, 2, 3, 4, 5, 6, 7, 8);
        },
        () => {
            context.copyTexSubImage2D(1, 2, 3, 4, 5, 6, 7, 8);
        },
        () => {
            context.createBuffer();
        },
        () => {
            context.createFramebuffer();
        },
        () => {
            context.createProgram();
        },
        () => {
            context.createRenderbuffer();
        },
        () => {
            context.createShader(1);
        },
        () => {
            context.createTexture();
        },
        () => {
            context.cullFace(1);
        },
        () => {
            context.deleteBuffer(buffer);
        },
        () => {
            context.deleteFramebuffer(framebuffer);
        },
        () => {
            context.deleteProgram(program);
        },
        () => {
            context.deleteRenderbuffer(renderbuffer);
        },
        () => {
            context.deleteShader(shader);
        },
        () => {
            context.deleteTexture(texture);
        },
        () => {
            context.depthFunc(1);
        },
        () => {
            context.depthMask(true);
        },
        () => {
            context.depthRange(1, 2);
        },
        () => {
            context.detachShader(program, shader);
        },
        () => {
            context.disable(1);
        },
        () => {
            context.disableVertexAttribArray(1);
        },
        () => {
            context.drawArrays(1, 2, 3);
        },
        () => {
            context.drawElements(1, 2, 3, 4);
        },
        () => {
            context.enable(1);
        },
        () => {
            context.enableVertexAttribArray(1);
        },
        () => {
            context.finish();
        },
        () => {
            context.flush();
        },
        () => {
            context.framebufferRenderbuffer(1, 2, 3, renderbuffer);
        },
        () => {
            context.framebufferTexture2D(1, 2, 3, texture, 4);
        },
        () => {
            context.frontFace(1);
        },
        () => {
            context.generateMipmap(1);
        },
        () => {
            context.getActiveAttrib(program, 1);
        },
        () => {
            context.getActiveUniform(program, 1);
        },
        () => {
            context.getAttachedShaders(program);
        },
        () => {
            context.getAttribLocation(program, "test");
        },
        () => {
            context.getBufferParameter(1, 2);
        },
        () => {
            context.getContextAttributes();
        },
        () => {
            context.getError();
        },
        () => {
            context.getExtension("test");
        },
        () => {
            context.getFramebufferAttachmentParameter(1, 2, 3);
        },
        () => {
            context.getParameter(1);
        },
        () => {
            context.getProgramInfoLog(program);
        },
        () => {
            context.getProgramParameter(program, 1);
        },
        () => {
            context.getRenderbufferParameter(1, 2);
        },
        () => {
            context.getShaderInfoLog(shader);
        },
        () => {
            context.getShaderParameter(shader, 1);
        },
        () => {
            context.getShaderPrecisionFormat(1, 2);
        },
        () => {
            context.getShaderSource(shader);
        },
        () => {
            context.getSupportedExtensions();
        },
        () => {
            context.getTexParameter(1, 2);
        },
        () => {
            context.getUniform(program, uniformLocation);
        },
        () => {
            context.getUniformLocation(program, "test");
        },
        () => {
            context.getVertexAttrib(1, 2);
        },
        () => {
            context.getVertexAttribOffset(1, 2);
        },
        () => {
            context.hint(1, 2);
        },
        () => {
            context.isBuffer(buffer);
        },
        () => {
            context.isContextLost();
        },
        () => {
            context.isEnabled(1);
        },
        () => {
            context.isFramebuffer(framebuffer);
        },
        () => {
            context.isProgram(program);
        },
        () => {
            context.isRenderbuffer(renderbuffer);
        },
        () => {
            context.isShader(shader);
        },
        () => {
            context.isTexture(texture);
        },
        () => {
            context.lineWidth(1);
        },
        () => {
            context.linkProgram(program);
        },
        () => {
            context.pixelStorei(1, 2);
        },
        () => {
            context.polygonOffset(1, 2);
        },
        () => {
            context.readPixels(1, 2, 3, 4, 5, 6, float32Array);
        },
        () => {
            context.renderbufferStorage(1, 2, 3, 4);
        },
        () => {
            context.sampleCoverage(1, true);
        },
        () => {
            context.scissor(1, 2, 3, 4);
        },
        () => {
            context.shaderSource(shader, "test");
        },
        () => {
            context.stencilFunc(1, 2, 3);
        },
        () => {
            context.stencilFuncSeparate(1, 2, 3, 4);
        },
        () => {
            context.stencilMask(1);
        },
        () => {
            context.stencilMaskSeparate(1, 2);
        },
        () => {
            context.stencilOp(1, 2, 3);
        },
        () => {
            context.stencilOpSeparate(1, 2, 3, 4);
        },
        () => {
            context.texParameterf(1, 2, 3);
        },
        () => {
            context.texParameteri(1, 2, 3);
        },
        () => {
            context.uniform1f(uniformLocation, 1);
        },
        () => {
            context.uniform1fv(uniformLocation, float32Array);
        },
        () => {
            context.uniform1i(uniformLocation, 1);
        },
        () => {
            context.uniform1iv(uniformLocation, int32Array);
        },
        () => {
            context.uniform2f(uniformLocation, 1, 2);
        },
        () => {
            context.uniform2fv(uniformLocation, float32Array);
        },
        () => {
            context.uniform2i(uniformLocation, 1, 2);
        },
        () => {
            context.uniform2iv(uniformLocation, int32Array);
        },
        () => {
            context.uniform3f(uniformLocation, 1, 2, 3);
        },
        () => {
            context.uniform3fv(uniformLocation, float32Array);
        },
        () => {
            context.uniform3i(uniformLocation, 1, 2, 3);
        },
        () => {
            context.uniform3iv(uniformLocation, int32Array);
        },
        () => {
            context.uniform4f(uniformLocation, 1, 2, 3, 4);
        },
        () => {
            context.uniform4fv(uniformLocation, float32Array);
        },
        () => {
            context.uniform4i(uniformLocation, 1, 2, 3, 4);
        },
        () => {
            context.uniform4iv(uniformLocation, int32Array);
        },
        () => {
            context.uniformMatrix2fv(uniformLocation, true, float32Array);
        },
        () => {
            context.uniformMatrix3fv(uniformLocation, true, float32Array);
        },
        () => {
            context.uniformMatrix4fv(uniformLocation, true, float32Array);
        },
        () => {
            context.useProgram(program);
        },
        () => {
            context.validateProgram(program);
        },
        () => {
            context.vertexAttrib1f(1, 2);
        },
        () => {
            context.vertexAttrib1fv(1, float32Array);
        },
        () => {
            context.vertexAttrib2f(1, 2, 3);
        },
        () => {
            context.vertexAttrib2fv(1, float32Array);
        },
        () => {
            context.vertexAttrib3f(1, 2, 3, 4);
        },
        () => {
            context.vertexAttrib3fv(1, float32Array);
        },
        () => {
            context.vertexAttrib4f(1, 2, 3, 4, 5);
        },
        () => {
            context.vertexAttrib4fv(1, float32Array);
        },
        () => {
            context.vertexAttribPointer(1, 2, 3, 4, 5, 6);
        },
        () => {
            context.viewport(1, 2, 3, 4);
        },
        () => {
            context.canvas.width;
            context.canvas.width = 2;
        },
        () => {
            context.canvas.height;
            context.canvas.height = 2;
        },
        () => {
            TestPage.dispatchEventToFrontend("LastFrame");
        },
    ];
    let index = 0;
    function executeFrameFunction() {
        frames[index++]();
        if (index < frames.length)
            requestAnimationFrameId = requestAnimationFrame(executeFrameFunction);
    };
    executeFrameFunction();
}

function performConsoleActions() {
    console.record(context, {name: "TEST"});

    context.createTexture();

    console.recordEnd(context);

    context.createBuffer();
}
