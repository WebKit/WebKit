description("Test of get calls against GL objects like getBufferParameter, etc.");

var gl = create3DContext();

var errorVert = gl.createShader(gl.VERTEX_SHADER);
gl.shaderSource(errorVert, "I am a bad vertex shader");
gl.compileShader(errorVert);

var errorFrag = gl.createShader(gl.FRAGMENT_SHADER);
gl.shaderSource(errorFrag, "I am a bad fragment shader");
gl.compileShader(errorFrag);

var standardVert = loadStandardVertexShader(gl);
var standardFrag = loadStandardFragmentShader(gl);
var standardProgram = gl.createProgram();
gl.attachShader(standardProgram, standardVert);
gl.attachShader(standardProgram, standardFrag);
gl.linkProgram(standardProgram);

// Test program and shader gets
var parseError = "ERROR: 0:1: 'I' : syntax error syntax error\nERROR: Parser found no code to compile in source strings.\n";
var errorVertString = "I am a bad vertex shader\n";
var errorFragString = "I am a bad fragment shader\n";
shouldBe('gl.getProgramInfoLog(standardProgram)', '""');
shouldBe('gl.getShaderInfoLog(errorVert)', 'parseError');
shouldBe('gl.getShaderInfoLog(errorFrag)', 'parseError');
shouldBe('gl.getShaderSource(errorVert)', 'errorVertString');
shouldBe('gl.getShaderSource(errorFrag)', 'errorFragString');

// Test getBufferParameter
var buffer = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
gl.bufferData(gl.ARRAY_BUFFER, 16, gl.DYNAMIC_DRAW);
shouldBe('gl.getBufferParameter(gl.ARRAY_BUFFER, gl.BUFFER_SIZE)', '16');
shouldBe('gl.getBufferParameter(gl.ARRAY_BUFFER, gl.BUFFER_USAGE)', 'gl.DYNAMIC_DRAW');

// Test getFramebufferAttachmentParameter
var texture = gl.createTexture();
gl.bindTexture(gl.TEXTURE_2D, texture);
gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 2, 2, 0, gl.RGBA, gl.UNSIGNED_BYTE,
              new WebGLUnsignedByteArray([0, 0, 0, 255,
                                          255, 255, 255, 255,
                                          255, 255, 255, 255,
                                          0, 0, 0, 255]));
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
gl.bindTexture(gl.TEXTURE_2D, null);
var framebuffer = gl.createFramebuffer();
gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, texture, 0);
var renderbuffer = gl.createRenderbuffer();
shouldBe('gl.getError()', '0');
gl.bindRenderbuffer(gl.RENDERBUFFER, renderbuffer);
shouldBe('gl.getError()', '0');
gl.renderbufferStorage(gl.RENDERBUFFER, gl.DEPTH_COMPONENT16, 2, 2);
shouldBe('gl.getError()', '0');
gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.RENDERBUFFER, renderbuffer);
// FIXME: on some machines (in particular the WebKit commit bots) the
// framebuffer status is FRAMEBUFFER_UNSUPPORTED; more investigation
// is needed why this is the case, because the FBO allocated
// internally by the WebKit implementation has almost identical
// parameters to this one. See https://bugs.webkit.org/show_bug.cgi?id=31843.
//shouldBe('gl.checkFramebufferStatus(gl.FRAMEBUFFER)', 'gl.FRAMEBUFFER_COMPLETE');
shouldBe('gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)', 'gl.TEXTURE');
shouldBe('gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.FRAMEBUFFER_ATTACHMENT_OBJECT_NAME)', 'texture');
shouldBe('gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL)', '0');
shouldBe('gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE)', '0');

shouldBe('gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)', 'gl.RENDERBUFFER');
shouldBe('gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.FRAMEBUFFER_ATTACHMENT_OBJECT_NAME)', 'renderbuffer');

// Test getProgramParameter
shouldBe('gl.getProgramParameter(standardProgram, gl.DELETE_STATUS)', 'false');
shouldBe('gl.getProgramParameter(standardProgram, gl.LINK_STATUS)', 'true');
shouldBe('typeof gl.getProgramParameter(standardProgram, gl.VALIDATE_STATUS)', '"boolean"');
shouldBe('typeof gl.getProgramParameter(standardProgram, gl.INFO_LOG_LENGTH)', '"number"');
shouldBe('gl.getProgramParameter(standardProgram, gl.ATTACHED_SHADERS)', '2');
shouldBe('gl.getProgramParameter(standardProgram, gl.ACTIVE_ATTRIBUTES)', '2');
shouldBeNonZero('gl.getProgramParameter(standardProgram, gl.ACTIVE_ATTRIBUTE_MAX_LENGTH)');
shouldBe('gl.getProgramParameter(standardProgram, gl.ACTIVE_UNIFORMS)', '1');
shouldBeNonZero('gl.getProgramParameter(standardProgram, gl.ACTIVE_UNIFORM_MAX_LENGTH)');

// Test getRenderbufferParameter
shouldBe('gl.getRenderbufferParameter(gl.RENDERBUFFER, gl.RENDERBUFFER_WIDTH)', '2');
shouldBe('gl.getRenderbufferParameter(gl.RENDERBUFFER, gl.RENDERBUFFER_HEIGHT)', '2');
// Note: we can't test the actual value of the internal format since
// the implementation is allowed to change it.
shouldBeNonZero('gl.getRenderbufferParameter(gl.RENDERBUFFER, gl.RENDERBUFFER_INTERNAL_FORMAT)');
shouldBeNonZero('gl.getRenderbufferParameter(gl.RENDERBUFFER, gl.RENDERBUFFER_DEPTH_SIZE)');
var colorbuffer = gl.createRenderbuffer();
shouldBe('gl.getError()', '0');
gl.bindRenderbuffer(gl.RENDERBUFFER, renderbuffer);
shouldBe('gl.getError()', '0');
gl.renderbufferStorage(gl.RENDERBUFFER, gl.RGBA4, 2, 2);
shouldBeNonZero('gl.getRenderbufferParameter(gl.RENDERBUFFER, gl.RENDERBUFFER_RED_SIZE)');
shouldBeNonZero('gl.getRenderbufferParameter(gl.RENDERBUFFER, gl.RENDERBUFFER_GREEN_SIZE)');
shouldBeNonZero('gl.getRenderbufferParameter(gl.RENDERBUFFER, gl.RENDERBUFFER_BLUE_SIZE)');
shouldBeNonZero('gl.getRenderbufferParameter(gl.RENDERBUFFER, gl.RENDERBUFFER_ALPHA_SIZE)');

// Test getShaderParameter
shouldBe('gl.getShaderParameter(standardVert, gl.SHADER_TYPE)', 'gl.VERTEX_SHADER');
shouldBe('gl.getShaderParameter(standardVert, gl.DELETE_STATUS)', 'false');
shouldBe('gl.getShaderParameter(standardVert, gl.COMPILE_STATUS)', 'true');
shouldBe('typeof gl.getShaderParameter(standardVert, gl.INFO_LOG_LENGTH)', '"number"');
shouldBeNonZero('gl.getShaderParameter(standardVert, gl.SHADER_SOURCE_LENGTH)');

// Test getTexParameter
gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
gl.bindTexture(gl.TEXTURE_2D, texture);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
shouldBe('gl.getTexParameter(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER)', 'gl.NEAREST');
shouldBe('gl.getTexParameter(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER)', 'gl.NEAREST');
shouldBe('gl.getTexParameter(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S)', 'gl.CLAMP_TO_EDGE');
shouldBe('gl.getTexParameter(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T)', 'gl.CLAMP_TO_EDGE');

// Test getUniform with all variants of data types
// Boolean uniform variables
var boolProgram = loadProgram(gl, "resources/boolUniformShader.vert", "resources/noopUniformShader.frag");
shouldBe('gl.getProgramParameter(boolProgram, gl.LINK_STATUS)', 'true');
var bvalLoc = gl.getUniformLocation(boolProgram, "bval");
var bval2Loc = gl.getUniformLocation(boolProgram, "bval2");
var bval3Loc = gl.getUniformLocation(boolProgram, "bval3");
var bval4Loc = gl.getUniformLocation(boolProgram, "bval4");
gl.useProgram(boolProgram);
gl.uniform1i(bvalLoc, 1);
gl.uniform2i(bval2Loc, 1, 0);
gl.uniform3i(bval3Loc, 1, 0, 1);
gl.uniform4i(bval4Loc, 1, 0, 1, 0);
shouldBe('gl.getError()', '0');
shouldBe('gl.getUniform(boolProgram, bvalLoc)', 'true');
shouldBe('gl.getUniform(boolProgram, bval2Loc)', '[1, 0]');
shouldBe('gl.getUniform(boolProgram, bval3Loc)', '[1, 0, 1]');
shouldBe('gl.getUniform(boolProgram, bval4Loc)', '[1, 0, 1, 0]');
// Integer uniform variables
var intProgram = loadProgram(gl, "resources/intUniformShader.vert", "resources/noopUniformShader.frag");
shouldBe('gl.getProgramParameter(intProgram, gl.LINK_STATUS)', 'true');
var ivalLoc = gl.getUniformLocation(intProgram, "ival");
var ival2Loc = gl.getUniformLocation(intProgram, "ival2");
var ival3Loc = gl.getUniformLocation(intProgram, "ival3");
var ival4Loc = gl.getUniformLocation(intProgram, "ival4");
gl.useProgram(intProgram);
gl.uniform1i(ivalLoc, 1);
gl.uniform2i(ival2Loc, 2, 3);
gl.uniform3i(ival3Loc, 4, 5, 6);
gl.uniform4i(ival4Loc, 7, 8, 9, 10);
shouldBe('gl.getError()', '0');
shouldBe('gl.getUniform(intProgram, ivalLoc)', '1');
shouldBe('gl.getUniform(intProgram, ival2Loc)', '[2, 3]');
shouldBe('gl.getUniform(intProgram, ival3Loc)', '[4, 5, 6]');
shouldBe('gl.getUniform(intProgram, ival4Loc)', '[7, 8, 9, 10]');
// Float uniform variables
var floatProgram = loadProgram(gl, "resources/floatUniformShader.vert", "resources/noopUniformShader.frag");
shouldBe('gl.getProgramParameter(floatProgram, gl.LINK_STATUS)', 'true');
var fvalLoc = gl.getUniformLocation(floatProgram, "fval");
var fval2Loc = gl.getUniformLocation(floatProgram, "fval2");
var fval3Loc = gl.getUniformLocation(floatProgram, "fval3");
var fval4Loc = gl.getUniformLocation(floatProgram, "fval4");
gl.useProgram(floatProgram);
gl.uniform1f(fvalLoc, 11);
gl.uniform2f(fval2Loc, 12, 13);
gl.uniform3f(fval3Loc, 14, 15, 16);
gl.uniform4f(fval4Loc, 17, 18, 19, 20);
shouldBe('gl.getError()', '0');
shouldBe('gl.getUniform(floatProgram, fvalLoc)', '11');
shouldBe('gl.getUniform(floatProgram, fval2Loc)', '[12, 13]');
shouldBe('gl.getUniform(floatProgram, fval3Loc)', '[14, 15, 16]');
shouldBe('gl.getUniform(floatProgram, fval4Loc)', '[17, 18, 19, 20]');
// Matrix uniform variables
var matProgram = loadProgram(gl, "resources/matUniformShader.vert", "resources/noopUniformShader.frag");
shouldBe('gl.getProgramParameter(matProgram, gl.LINK_STATUS)', 'true');
var mval2Loc = gl.getUniformLocation(matProgram, "mval2");
var mval3Loc = gl.getUniformLocation(matProgram, "mval3");
var mval4Loc = gl.getUniformLocation(matProgram, "mval4");
gl.useProgram(matProgram);
gl.uniformMatrix2fv(mval2Loc, false, [1, 2, 3, 4]);
gl.uniformMatrix3fv(mval3Loc, false, [5, 6, 7, 8, 9, 10, 11, 12, 13]);
gl.uniformMatrix4fv(mval4Loc, false, [14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29]);
shouldBe('gl.getError()', '0');
shouldBe('gl.getUniform(matProgram, mval2Loc)', '[1, 2, 3, 4]');
shouldBe('gl.getUniform(matProgram, mval3Loc)', '[5, 6, 7, 8, 9, 10, 11, 12, 13]');
shouldBe('gl.getUniform(matProgram, mval4Loc)', '[14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29]');

// Test getVertexAttrib
var array = new WebGLFloatArray([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
gl.bufferData(gl.ARRAY_BUFFER, array, gl.DYNAMIC_DRAW);
// Vertex attribute 0 is special in that it has no current state, so
// fetching GL_CURRENT_VERTEX_ATTRIB generates an error. Use attribute
// 1 for these tests instead.
gl.enableVertexAttribArray(1);
gl.vertexAttribPointer(1, 4, gl.FLOAT, false, 0, 0);
shouldBeNonNull('gl.getVertexAttrib(1, gl.VERTEX_ATTRIB_ARRAY_BUFFER_BINDING)');
shouldBe('gl.getVertexAttrib(1, gl.VERTEX_ATTRIB_ARRAY_ENABLED)', 'true');
shouldBe('gl.getVertexAttrib(1, gl.VERTEX_ATTRIB_ARRAY_SIZE)', '4');
shouldBe('(gl.getVertexAttrib(1, gl.VERTEX_ATTRIB_ARRAY_STRIDE) == 0) || (gl.getVertexAttrib(1, gl.VERTEX_ATTRIB_ARRAY_STRIDE) == 4 * gl.sizeInBytes(gl.FLOAT))', 'true');
shouldBe('gl.getVertexAttrib(1, gl.VERTEX_ATTRIB_ARRAY_TYPE)', 'gl.FLOAT');
shouldBe('gl.getVertexAttrib(1, gl.VERTEX_ATTRIB_ARRAY_NORMALIZED)', 'false');
gl.disableVertexAttribArray(1);
shouldBe('gl.getVertexAttrib(1, gl.VERTEX_ATTRIB_ARRAY_ENABLED)', 'false');
gl.vertexAttrib4f(1, 5, 6, 7, 8);
shouldBe('gl.getVertexAttrib(1, gl.CURRENT_VERTEX_ATTRIB)', '[5, 6, 7, 8]');
shouldBe('gl.getError()', '0');

successfullyParsed = true;
