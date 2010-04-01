description("Test of get calls against GL objects like getBufferParameter, etc.");

var gl = create3DContext({ antialias: false });
var program = loadStandardProgram(gl);

// 3 vertices => 1 triangle, interleaved data
var data = new WebGLFloatArray([0, 0, 0, 1,
                                0, 0, 1,
                                1, 0, 0, 1,
                                0, 0, 1,
                                1, 1, 1, 1,
                                0, 0, 1]);
var indices = new WebGLUnsignedShortArray([0, 1, 2]);

var buffer = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
gl.bufferData(gl.ARRAY_BUFFER, data, gl.STATIC_DRAW);
var elements = gl.createBuffer();
gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, elements);
gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW);
gl.useProgram(program);
var vertexLoc = gl.getAttribLocation(program, "a_vertex");
var normalLoc = gl.getAttribLocation(program, "a_normal");
gl.vertexAttribPointer(vertexLoc, 4, gl.FLOAT, false, 7 * gl.sizeInBytes(gl.FLOAT), 0);
gl.enableVertexAttribArray(vertexLoc);
gl.vertexAttribPointer(normalLoc, 3, gl.FLOAT, false, 7 * gl.sizeInBytes(gl.FLOAT), 3 * gl.sizeInBytes(gl.FLOAT));
gl.enableVertexAttribArray(normalLoc);
shouldBe('gl.getError()', '0');
shouldBeUndefined('gl.drawElements(gl.TRIANGLES, 3, gl.UNSIGNED_SHORT, 0)');
shouldBe('gl.getError()', '0');

successfullyParsed = true;
