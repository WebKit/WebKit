description("Test the behavior of CanvasRenderingContext2D.fillText() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var SyntaxError = "SyntaxError: Not enough arguments";
var TypeError = "TypeError: Type error";

shouldThrow("ctx.fillText()", "SyntaxError");
shouldThrow("ctx.fillText('moo')", "SyntaxError");
shouldThrow("ctx.fillText('moo',0)", "SyntaxError");
shouldBe("ctx.fillText('moo',0,0)", "undefined");
shouldBe("ctx.fillText('moo',0,0,0)", "undefined");
shouldBe("ctx.fillText('moo',0,0,0,0)", "undefined");

var successfullyParsed = true;
