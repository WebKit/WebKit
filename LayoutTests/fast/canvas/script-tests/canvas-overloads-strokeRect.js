description("Test the behavior of CanvasRenderingContext2D.strokeRect() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var SyntaxError = "SyntaxError: Syntax error";
var TypeError = "TypeError: Type error";

shouldBe("ctx.strokeRect()", "undefined");
shouldBe("ctx.strokeRect(0)", "undefined");
shouldBe("ctx.strokeRect(0, 0)", "undefined");
shouldBe("ctx.strokeRect(0, 0, 0)", "undefined");
shouldBe("ctx.strokeRect(0, 0, 0, 0)", "undefined");
shouldBe("ctx.strokeRect(0, 0, 0, 0, 0)", "undefined");
