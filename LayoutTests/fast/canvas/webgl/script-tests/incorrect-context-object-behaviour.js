description("Tests calling WebGL APIs with objects from other contexts");

var contextA = create3DContext();
var contextB = create3DContext();
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
shouldThrow("contextA.getProgrami(programB, 0)");
shouldThrow("contextA.getProgramiv(programB, 0)");
shouldThrow("contextA.getProgramInfoLog(programB, 0)");
shouldThrow("contextA.getShaderi(shaderB, 0)");
shouldThrow("contextA.getShaderiv(shaderB, 0)");
shouldThrow("contextA.getShaderInfoLog(shaderB, 0)");
shouldThrow("contextA.getShaderSource(shaderB)");
shouldThrow("contextA.getUniformf(programB, locationA)");
shouldThrow("contextA.getUniformfv(programB, locationA)");
shouldThrow("contextA.getUniformi(programB, locationA)");
shouldThrow("contextA.getUniformiv(programB, locationA)");
shouldThrow("contextA.getUniformLocation(programB, 'u_modelViewProjMatrix')");

successfullyParsed = true;
