description(
"This test checks that const declarations in JavaScript work and are readonly."
);

const x = "RIGHT";
x = "WRONG";
shouldBe("x", '"RIGHT"');

const x = "RIGHT", y = "RIGHT";
y = "WRONG";
shouldBe("y", '"RIGHT"');

var successfullyParsed = true;
