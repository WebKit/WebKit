description("Test the behavior of CanvasRenderingContext2D.setFillColor() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var TypeError = "TypeError: Type error";

shouldThrow("ctx.setFillColor()", "TypeError");
shouldBe("ctx.setFillColor('red')", "undefined");
shouldBe("ctx.setFillColor(0)", "undefined");
shouldBe("ctx.setFillColor(0, 0)", "undefined");
shouldThrow("ctx.setFillColor(0, 0, 0)", "TypeError");
shouldBe("ctx.setFillColor(0, 0, 0, 0)", "undefined");
shouldBe("ctx.setFillColor(0, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.setFillColor(0, 0, 0, 0, 0, 0)", "TypeError");
