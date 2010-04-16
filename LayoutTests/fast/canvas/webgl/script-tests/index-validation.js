description("Test of validating indices for drawElements().");

var gl = create3DContext();
var program = loadStandardProgram(gl);

// 3 vertices => 1 triangle, interleaved data
var dataComplete = new WebGLFloatArray([0, 0, 0, 1,
                                        0, 0, 1,
                                        1, 0, 0, 1,
                                        0, 0, 1,
                                        1, 1, 1, 1,
                                        0, 0, 1]);
var dataIncomplete = new WebGLFloatArray([0, 0, 0, 1,
                                          0, 0, 1,
                                          1, 0, 0, 1,
                                          0, 0, 1,
                                          1, 1, 1, 1]);
var indices = new WebGLUnsignedShortArray([0, 1, 2]);

debug("Testing with valid indices");

var bufferComplete = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, bufferComplete);
gl.bufferData(gl.ARRAY_BUFFER, dataComplete, gl.STATIC_DRAW);
var elements = gl.createBuffer();
gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, elements);
gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW);
gl.useProgram(program);
var vertexLoc = gl.getAttribLocation(program, "a_vertex");
var normalLoc = gl.getAttribLocation(program, "a_normal");
gl.vertexAttribPointer(vertexLoc, 4, gl.FLOAT, false, 7 * gl.sizeInBytes(gl.FLOAT), 0);
gl.enableVertexAttribArray(vertexLoc);
gl.vertexAttribPointer(normalLoc, 3, gl.FLOAT, false, 7 * gl.sizeInBytes(gl.FLOAT), 4 * gl.sizeInBytes(gl.FLOAT));
gl.enableVertexAttribArray(normalLoc);
shouldBe('gl.checkFramebufferStatus(gl.FRAMEBUFFER)', 'gl.FRAMEBUFFER_COMPLETE');
shouldBe('gl.getError()', '0');
shouldBeUndefined('gl.drawElements(gl.TRIANGLES, 3, gl.UNSIGNED_SHORT, 0)');
shouldBe('gl.getError()', '0');

debug("Testing with out-of-range indices");

var bufferIncomplete = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, bufferIncomplete);
gl.bufferData(gl.ARRAY_BUFFER, dataIncomplete, gl.STATIC_DRAW);
gl.vertexAttribPointer(vertexLoc, 4, gl.FLOAT, false, 7 * gl.sizeInBytes(gl.FLOAT), 0);
gl.enableVertexAttribArray(vertexLoc);
gl.disableVertexAttribArray(normalLoc);
debug("Enable vertices, valid");
shouldBe('gl.getError()', '0');
shouldBeUndefined('gl.drawElements(gl.TRIANGLES, 3, gl.UNSIGNED_SHORT, 0)');
shouldBe('gl.getError()', '0');
debug("Enable normals, out-of-range");
gl.vertexAttribPointer(normalLoc, 3, gl.FLOAT, false, 7 * gl.sizeInBytes(gl.FLOAT), 4 * gl.sizeInBytes(gl.FLOAT));
gl.enableVertexAttribArray(normalLoc);
shouldBe('gl.getError()', '0');
shouldBeUndefined('gl.drawElements(gl.TRIANGLES, 3, gl.UNSIGNED_SHORT, 0)');
shouldBe('gl.getError()', 'gl.INVALID_OPERATION');

debug("Test with enabled attribute that does not belong to current program");

gl.disableVertexAttribArray(normalLoc);
var extraLoc = Math.max(vertexLoc, normalLoc) + 1;
gl.enableVertexAttribArray(extraLoc);
debug("Enable an extra attribute with null");
shouldBe('gl.getError()', '0');
shouldBeUndefined('gl.drawElements(gl.TRIANGLES, 3, gl.UNSIGNED_SHORT, 0)');
shouldBe('gl.getError()', '0');
debug("Enable an extra attribute with insufficient data buffer");
gl.vertexAttribPointer(extraLoc, 3, gl.FLOAT, false, 7 * gl.sizeInBytes(gl.FLOAT), 4 * gl.sizeInBytes(gl.FLOAT));
shouldBe('gl.getError()', '0');
shouldBeUndefined('gl.drawElements(gl.TRIANGLES, 3, gl.UNSIGNED_SHORT, 0)');
shouldBe('gl.getError()', '0');

successfullyParsed = true;
