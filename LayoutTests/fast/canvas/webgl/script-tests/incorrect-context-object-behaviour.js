description("Tests calling WebGL APIs with objects from other contexts");

var contextA = create3DDebugContext();
var contextB = create3DDebugContext();
var programA = loadStandardProgram(contextA);
var programB = loadStandardProgram(contextB);
var shaderA = loadStandardVertexShader(contextA);
var shaderB = loadStandardVertexShader(contextB);
var textureA = contextA.createTexture();
var textureB = contextB.createTexture();
var frameBufferA = contextA.createFramebuffer();
var frameBufferB = contextB.createFramebuffer();
var renderBufferA = contextA.createRenderbuffer();
var renderBufferB = contextB.createRenderbuffer();
var locationA = contextA.getUniformLocation(programA, 'u_modelViewProjMatrix');
var locationB = contextB.getUniformLocation(programB, 'u_modelViewProjMatrix');


shouldThrow("contextA.compileShader(shaderB)");
shouldThrow("contextA.linkProgram(programB)");
shouldThrow("contextA.attachShader(programA, shaderB)");
shouldThrow("contextA.attachShader(programB, shaderA)");
shouldThrow("contextA.attachShader(programB, shaderB)");
shouldThrow("contextA.detachShader(programA, shaderB)");
shouldThrow("contextA.detachShader(programB, shaderA)");
shouldThrow("contextA.detachShader(programB, shaderB)");
shouldThrow("contextA.shaderSource(shaderB, 'foo')");
shouldThrow("contextA.bindAttribLocation(programB, 0, 'foo')");
shouldThrow("contextA.bindFramebuffer(contextA.FRAMEBUFFER, frameBufferB)");
shouldThrow("contextA.bindRenderbuffer(contextA.RENDERBUFFER, renderBufferB)");
shouldThrow("contextA.bindTexture(contextA.TEXTURE_2D, textureB)");
shouldThrow("contextA.framebufferRenderbuffer(contextA.FRAMEBUFFER, contextA.DEPTH_ATTACHMENT, contextA.RENDERBUFFER, renderBufferB)");
shouldThrow("contextA.framebufferTexture2D(contextA.FRAMEBUFFER, contextA.COLOR_ATTACHMENT0, contextA.TEXTURE_2D, textureB, 0)");
shouldThrow("contextA.getProgramParameter(programB, 0)");
shouldThrow("contextA.getProgramInfoLog(programB, 0)");
shouldThrow("contextA.getShaderParameter(shaderB, 0)");
shouldThrow("contextA.getShaderInfoLog(shaderB, 0)");
shouldThrow("contextA.getShaderSource(shaderB)");
shouldThrow("contextA.getUniform(programB, locationA)");
shouldThrow("contextA.getUniformLocation(programB, 'u_modelViewProjMatrix')");

successfullyParsed = true;
