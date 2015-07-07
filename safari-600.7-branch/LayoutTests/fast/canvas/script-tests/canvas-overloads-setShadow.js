description("Test the behavior of CanvasRenderingContext2D.setShadow() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var TypeError = "TypeError: Type error";
var TypeErrorNotEnoughArguments = "TypeError: Not enough arguments";

shouldThrow("ctx.setShadow()", "TypeErrorNotEnoughArguments");
shouldThrow("ctx.setShadow(0)", "TypeErrorNotEnoughArguments");
shouldThrow("ctx.setShadow(0, 0)", "TypeErrorNotEnoughArguments");
shouldBe("ctx.setShadow(0, 0, 0)", "undefined");
shouldBe("ctx.setShadow(0, 0, 0, 0)", "undefined");
shouldBe("ctx.setShadow(0, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.setShadow(0, 0, 0, 0, 0, 0)", "TypeError");
shouldBe("ctx.setShadow(0, 0, 0, 0, 'red')", "undefined");
shouldThrow("ctx.setShadow(0, 0, 0, 0, 'red', 0)", "TypeError");
shouldBe("ctx.setShadow(0, 0, 0, 0, 'red', 0, 0)", "undefined");
shouldThrow("ctx.setShadow(0, 0, 0, 0, 0, 0)", "TypeError");
shouldBe("ctx.setShadow(0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.setShadow(0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.setShadow(0, 0, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
