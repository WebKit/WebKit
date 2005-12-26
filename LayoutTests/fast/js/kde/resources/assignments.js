// simple assignment
shouldBe("var i = 1; i", "1");
shouldBe("j = k = 2", "2");
shouldBeUndefined("var i; i");

// compound assignments
shouldBe("var i = 1; i <<= 2", "4");
shouldBe("var i = 8; i >>= 1", "4");
shouldBe("var i = 1; i >>= 2", "0");
shouldBe("var i = -8; i >>= 24", "-1");
shouldBe("var i = 8; i >>>= 2", "2");
shouldBe("var i = -8; i >>>= 24", "255");
successfullyParsed = true
