description("Tests calling WebGL APIs without providing the necessary objects");

var context = create3DContext();
var program = loadStandardProgram(context);
var shader = loadStandardVertexShader(context);

shouldThrow("context.compileShader()");
shouldThrow("context.linkProgram()");
shouldThrow("context.attachShader()");
shouldThrow("context.attachShader(program, undefined)");
shouldThrow("context.attachShader(undefined, shader)");
shouldThrow("context.detachShader(program, undefined)");
shouldThrow("context.detachShader(undefined, shader)");
shouldThrow("context.shaderSource()");
shouldThrow("context.shaderSource(undefined, 'foo')");
shouldThrow("context.bindAttribLocation(undefined, 0, 'foo')");
shouldBeUndefined("context.bindBuffer(context.ARRAY_BUFFER, 0)");
shouldBeUndefined("context.bindFramebuffer(context.FRAMEBUFFER, 0)");
shouldBeUndefined("context.bindRenderbuffer(context.RENDERBUFFER, 0)");
shouldBeUndefined("context.bindTexture(context.TEXTURE_2D, 0)");
shouldBeUndefined("context.framebufferRenderbuffer(context.FRAMEBUFFER, context.DEPTH_ATTACHMENT, context.RENDERBUFFER, 0)");
shouldBeUndefined("context.framebufferTexture2D(context.FRAMEBUFFER, context.COLOR_ATTACHMENT0, context.TEXTURE_2D, 0, 0)");
shouldThrow("context.getProgrami(undefined, 0)");
shouldThrow("context.getProgramiv(undefined, 0)");
shouldThrow("context.getProgramInfoLog(undefined, 0)");
shouldThrow("context.getShaderi(undefined, 0)");
shouldThrow("context.getShaderiv(undefined, 0)");
shouldThrow("context.getShaderInfoLog(undefined, 0)");
shouldThrow("context.getShaderSource(undefined)");
shouldThrow("context.getUniformf(undefined, 0)");
shouldThrow("context.getUniformfv(undefined, 0)");
shouldThrow("context.getUniformi(undefined, 0)");
shouldThrow("context.getUniformiv(undefined, 0)");
shouldThrow("context.getUniformLocation(undefined, 'foo')");

successfullyParsed = true;
