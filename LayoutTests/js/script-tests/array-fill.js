description(
"This test checks the behavior of the Array.prototype.fill()"
);

shouldBe("[0, 0, 0, 0, 0].fill()", "[undefined, undefined, undefined, undefined, undefined]");
shouldBe("[0, 0, 0, 0, 0].fill(3)", "[3, 3, 3, 3, 3]");
shouldBe("[0, 0, 0, 0, 0].fill(3, 1)", "[0, 3, 3, 3, 3]");
shouldBe("[0, 0, 0, 0, 0].fill(3, 1, 3)", "[0, 3, 3, 0, 0]");
shouldBe("[0, 0, 0, 0, 0].fill(3, 1, 1000)", "[0, 3, 3, 3, 3]");
shouldBe("[0, 0, 0, 0, 0].fill(3, -2, 1000)", "[0, 0, 0, 3, 3]");
shouldBe("[0, 0, 0, 0, 0].fill(3, -2, 4)", "[0, 0, 0, 3, 0]");
shouldBe("[0, 0, 0, 0, 0].fill(3, -2, -1)", "[0, 0, 0, 3, 0]");
shouldBe("[0, 0, 0, 0, 0].fill(3, -2, -3)", "[0, 0, 0, 0, 0]");
shouldBe("[0, 0, 0, 0, 0].fill(3, undefined, 4)", "[3, 3, 3, 3, 0]");
shouldBe("[ ,  ,  ,  , 0].fill(3, 1, 3)", "[, 3, 3, , 0]");


successfullyParsed = true;
