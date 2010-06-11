description("Test of getActiveAttrib and getActiveUniform");

var context = create3DContext();
shouldBeNonNull("context");
var context2 = create3DContext();
shouldBeNonNull("context2");
var program = loadStandardProgram(context);
shouldBeNonNull("program");
var program2 = loadProgram(context2,
                           "resources/intArrayUniformShader.vert",
                           "resources/noopUniformShader.frag");
shouldBeNonNull("program2");
shouldBe("context.getError()", "context.NO_ERROR");

shouldBe("context.getActiveUniform(program, 0).name", "'u_modelViewProjMatrix'");
shouldBe("context.getActiveUniform(program, 0).type", "context.FLOAT_MAT4");
shouldBe("context.getActiveUniform(program, 0).size", "1");
shouldBeNull("context.getActiveUniform(program, 1)");
shouldBe("context.getError()", "context.INVALID_VALUE");
shouldBeNull("context.getActiveUniform(program, -1)");
shouldBe("context.getError()", "context.INVALID_VALUE");
shouldBeNull("context.getActiveUniform(null, 0)");
shouldBe("context.getError()", "context.INVALID_VALUE");

// we don't know the order the attribs will appear.
var info = [
    context.getActiveAttrib(program, 0),
    context.getActiveAttrib(program, 1)
];
for (var ii = 0; ii < info.length; ++ii)
    shouldBeNonNull("info[ii]");

var expected = [
    { name: 'a_normal', type: context.FLOAT_VEC3, size: 1 },
    { name: 'a_vertex', type: context.FLOAT_VEC4, size: 1 }
];

if (info[0].name != expected[0].name) {
    t = info[0];
    info[0] = info[1];
    info[1] = t;
}

for (var ii = 0; ii < info.length; ++ii) {
    shouldBe("info[ii].name", "expected[ii].name");
    shouldBe("info[ii].type", "expected[ii].type");
    shouldBe("info[ii].size", "expected[ii].size");
}

// we don't know the order the uniforms will appear.
var info2 = [
    context2.getActiveUniform(program2, 0),
    context2.getActiveUniform(program2, 1)
];
for (var ii = 0; ii < info2.length; ++ii)
    shouldBeNonNull("info2[ii]");

var expected2 = [
    { name: 'ival', type: context.INT, size: 1 },
    { name: 'ival2[0]', type: context.INT, size: 2 }
];

if (info2[0].name != expected2[0].name) {
    t = info2[0];
    info2[0] = info2[1];
    info2[1] = t;
}

for (var ii = 0; ii < info.length; ++ii) {
    shouldBe("info2[ii].name", "expected2[ii].name");
    shouldBe("info2[ii].type", "expected2[ii].type");
    shouldBe("info2[ii].size", "expected2[ii].size");
}

shouldBeNull("context.getActiveAttrib(program, 2)");
shouldBe("context.getError()", "context.INVALID_VALUE");
shouldBeNull("context.getActiveAttrib(program, -1)");
shouldBe("context.getError()", "context.INVALID_VALUE");
shouldBeNull("context.getActiveAttrib(null, 0)");
shouldBe("context.getError()", "context.INVALID_VALUE");

shouldBe("context2.getError()", "context.NO_ERROR");
shouldBeNull("context2.getActiveAttrib(program, 0)");
shouldBe("context2.getError()", "context2.INVALID_OPERATION");
shouldBeNull("context2.getActiveUniform(program, 0)");
shouldBe("context2.getError()", "context2.INVALID_OPERATION");

context.deleteProgram(program);
shouldBeNull("context.getActiveUniform(program, 0)");
shouldBe("context.getError()", "context.INVALID_VALUE");
shouldBeNull("context.getActiveAttrib(program, 0)");
shouldBe("context.getError()", "context.INVALID_VALUE");

successfullyParsed = true;