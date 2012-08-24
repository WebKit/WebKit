description("Test the behavior of CanvasRenderingContext2D.setFillColor() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var TypeError = "TypeError: Type error";
var TypeErrorNotEnoughArguments = "TypeError: Not enough arguments";

shouldThrow("ctx.setFillColor()", "TypeErrorNotEnoughArguments");
shouldBe("ctx.setFillColor('red')", "undefined");
shouldBe("ctx.setFillColor(0)", "undefined");
shouldBe("ctx.setFillColor(0, 0)", "undefined");
shouldThrow("ctx.setFillColor(0, 0, 0)", "TypeError");
shouldBe("ctx.setFillColor(0, 0, 0, 0)", "undefined");
shouldBe("ctx.setFillColor(0, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.setFillColor(0, 0, 0, 0, 0, 0)", "TypeError");
