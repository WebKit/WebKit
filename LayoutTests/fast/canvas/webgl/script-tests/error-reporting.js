description("Tests generation of synthetic and real GL errors");

var context = create3DContext();
var program = loadStandardProgram(context);

// Other tests in this directory like getActiveTest and
// incorrect-context-object-behaviour already test the raising of many
// synthetic GL errors. This test verifies the raising of certain
// known real GL errors, and contains a few regression tests for bugs
// discovered in the synthetic error generation and in the WebGL
// implementation itself.

shouldBe("context.getError()", "0");

debug("Testing getActiveAttrib");
// Synthetic OpenGL error
shouldBeNull("context.getActiveAttrib(null, 2)");
shouldBe("context.getError()", "context.INVALID_VALUE");
// Error state should be clear by this point
shouldBe("context.getError()", "context.NO_ERROR");
// Real OpenGL error
shouldBeNull("context.getActiveAttrib(program, 2)");
shouldBe("context.getError()", "context.INVALID_VALUE");
// Error state should be clear by this point
shouldBe("context.getError()", "context.NO_ERROR");

debug("Testing getActiveUniform");
// Synthetic OpenGL error
shouldBeNull("context.getActiveUniform(null, 0)");
shouldBe("context.getError()", "context.INVALID_VALUE");
// Error state should be clear by this point
shouldBe("context.getError()", "context.NO_ERROR");
// Real OpenGL error
shouldBeNull("context.getActiveUniform(program, 50)");
shouldBe("context.getError()", "context.INVALID_VALUE");
// Error state should be clear by this point
shouldBe("context.getError()", "context.NO_ERROR");

debug("Testing attempts to manipulate the default framebuffer");
shouldBeUndefined("context.bindFramebuffer(context.FRAMEBUFFER, 0)");
shouldBe("context.getError()", "context.NO_ERROR");
shouldBeUndefined("context.framebufferRenderbuffer(context.FRAMEBUFFER, context.DEPTH_ATTACHMENT, context.RENDERBUFFER, 0)");
// Synthetic OpenGL error
shouldBe("context.getError()", "context.INVALID_OPERATION");
// Error state should be clear by this point
shouldBe("context.getError()", "context.NO_ERROR");
shouldBeUndefined("context.framebufferTexture2D(context.FRAMEBUFFER, context.COLOR_ATTACHMENT0, context.TEXTURE_2D, 0, 0)");
// Synthetic OpenGL error
shouldBe("context.getError()", "context.INVALID_OPERATION");
// Error state should be clear by this point
shouldBe("context.getError()", "context.NO_ERROR");

successfullyParsed = true;
