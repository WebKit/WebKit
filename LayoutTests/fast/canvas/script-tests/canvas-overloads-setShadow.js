description("Test the behavior of CanvasRenderingContext2D.setShadow() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var TypeError = "TypeError: Type error";

shouldThrow("ctx.setShadow()", "TypeError");
shouldThrow("ctx.setShadow(0)", "TypeError");
shouldThrow("ctx.setShadow(0, 0)", "TypeError");
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
