shouldBe("Boolean()", "false");
shouldBe("Boolean(true)", "true");
shouldBe("Boolean(false)", "false");
shouldBe("(new Boolean(true)).valueOf()", "true");
shouldBe("(new Boolean(false)).valueOf()", "false");
shouldBe("(new Boolean(Boolean(true))).valueOf()", "true");
shouldBeTrue("true.valueOf() === true");
shouldBeTrue("false.toString() === 'false'");
successfullyParsed = true
