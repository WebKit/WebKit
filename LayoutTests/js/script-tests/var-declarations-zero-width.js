description("This page tests if U+200C and U+200D are allowed as part of an identifier.");

shouldBe("var x\u200c = 42; x\u200c", "42");
shouldBe("var x\u200d = 43; x\u200d", "43");
shouldBe("var x\u200c\u200d = 44; x\u200c\u200d", "44");