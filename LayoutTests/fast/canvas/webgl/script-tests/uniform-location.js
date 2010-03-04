description("Tests WebGL APIs related to shader uniforms");

var contextA = create3DDebugContext();
var contextB = create3DDebugContext();
var programA1 = loadStandardProgram(contextA);
var programA2 = loadStandardProgram(contextA);
var programB = loadStandardProgram(contextB);
var programS = loadProgram(contextA, "resources/structUniformShader.vert", "resources/fragmentShader.frag");
var programV = loadProgram(contextA, "resources/floatUniformShader.vert", "resources/noopUniformShader.frag");
var locationA = contextA.getUniformLocation(programA1, 'u_modelViewProjMatrix');
var locationB = contextB.getUniformLocation(programB, 'u_modelViewProjMatrix');
var locationSx = contextA.getUniformLocation(programS, "u_struct.x");
var locationArray0 = contextA.getUniformLocation(programS, "u_array[0]");
var locationVec4 = contextA.getUniformLocation(programV, "fval4");

var vec = [1, 2, 3, 4];
var mat = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];

shouldBeUndefined("contextA.useProgram(programA2)");
shouldThrow("contextA.uniformMatrix4fv(locationA, false, mat)");
shouldBeUndefined("contextA.useProgram(programA1)");
shouldBeUndefined("contextA.uniformMatrix4fv(locationA, false, mat)");
shouldThrow("contextA.uniformMatrix4fv(0, false, mat)");

shouldBeUndefined("contextA.useProgram(programS)");
shouldBeUndefined("contextA.uniform1i(locationSx, 3)");
shouldBeUndefined("contextA.uniform1f(locationArray0, 4.0)");

shouldBe("contextA.getUniform(programS, locationSx)", "3");
shouldBe("contextA.getUniform(programS, locationArray0)", "4.0");

shouldBeUndefined("contextA.useProgram(programV)");
shouldBeUndefined("contextA.uniform4fv(locationVec4, vec)");
shouldBe("contextA.getUniform(programV, locationVec4)", "vec");

shouldBeNull("contextA.getUniformLocation(programV, \"IDontExist\")");

successfullyParsed = true;
