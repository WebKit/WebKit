description("Test of getActiveAttrib and getActiveUniform");

var context = create3DContext();
var context2 = create3DContext();
var program = loadStandardProgram(context);
var program2 = loadStandardProgram(context2);

shouldBe("context.getError()", "0");
shouldBe("context.getActiveUniform(program, 0).name", "'u_modelViewProjMatrix'");
shouldBe("context.getActiveUniform(program, 0).type", "context.FLOAT_MAT4");
shouldBe("context.getActiveUniform(program, 0).size", "1");
shouldThrow("context.getActiveUniform(program, 1)");
shouldBe("context.getError()", "context.INVALID_VALUE");
shouldThrow("context.getActiveUniform(program, -1)");
shouldBe("context.getError()", "context.INVALID_VALUE");
shouldThrow("context.getActiveUniform(null, 0)");
shouldBe("context.getError()", "0");

shouldBe("context.getActiveAttrib(program, 0).name", "'a_normal'");
shouldBe("context.getActiveAttrib(program, 0).type", "context.FLOAT_VEC3");
shouldBe("context.getActiveAttrib(program, 0).size", "1");
shouldBe("context.getActiveAttrib(program, 1).name", "'a_vertex'");
shouldBe("context.getActiveAttrib(program, 1).type", "context.FLOAT_VEC4");
shouldBe("context.getActiveAttrib(program, 1).size", "1");
shouldThrow("context.getActiveAttrib(program, 2)");
shouldBe("context.getError()", "context.INVALID_VALUE");
shouldThrow("context.getActiveAttrib(program, -1)");
shouldBe("context.getError()", "context.INVALID_VALUE");
shouldThrow("context.getActiveAttrib(null, 0)");
shouldBe("context.getError()", "0");

shouldBe("context2.getError()", "0");
shouldThrow("context2.getActiveAttrib(program, 0)");
shouldBe("context2.getError()", "0");
shouldThrow("context2.getActiveUniform(program, 0)");
shouldBe("context2.getError()", "0");

context.deleteProgram(program);
shouldThrow("context.getActiveUniform(program, 0)");
shouldBe("context.getError()", "0");
shouldThrow("context.getActiveAttrib(program, 0)");
shouldBe("context.getError()", "0");

successfullyParsed = true;
