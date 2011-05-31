description("Test the behavior of CanvasRenderingContext2D.strokeText() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var SyntaxError = "SyntaxError: Not enough arguments";

shouldThrow("ctx.strokeText()", "SyntaxError");
shouldThrow("ctx.strokeText('moo')", "SyntaxError");
shouldThrow("ctx.strokeText('moo',0)", "SyntaxError");
shouldBe("ctx.strokeText('moo',0,0)", "undefined");
shouldBe("ctx.strokeText('moo',0,0,0)", "undefined");
shouldBe("ctx.strokeText('moo',0,0,0,0)", "undefined");

var successfullyParsed = true;
