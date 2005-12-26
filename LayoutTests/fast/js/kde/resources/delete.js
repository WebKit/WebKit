shouldBe("a = 1; delete a;", "true");
shouldBe("delete nonexistant;", "true");
shouldBe("delete NaN", "false");

successfullyParsed = true
