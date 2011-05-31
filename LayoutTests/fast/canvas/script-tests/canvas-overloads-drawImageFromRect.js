description("Test the behavior of CanvasRenderingContext2D.drawImageFromRect() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var SyntaxError = "SyntaxError: Not enough arguments";

var imageElement = document.createElement("img");
shouldThrow("ctx.drawImageFromRect()", "SyntaxError");
shouldBe("ctx.drawImageFromRect(imageElement)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");

var successfullyParsed = true;
