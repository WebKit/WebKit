description("Test the behavior of CanvasRenderingContext2D.strokeRect() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var TypeErrorNotEnoughArguments = "TypeError: Not enough arguments";

shouldThrow("ctx.strokeRect()", "TypeErrorNotEnoughArguments");
shouldThrow("ctx.strokeRect(0)", "TypeErrorNotEnoughArguments");
shouldThrow("ctx.strokeRect(0, 0)", "TypeErrorNotEnoughArguments");
shouldThrow("ctx.strokeRect(0, 0, 0)", "TypeErrorNotEnoughArguments");
shouldBe("ctx.strokeRect(0, 0, 0, 0)", "undefined");
shouldBe("ctx.strokeRect(0, 0, 0, 0, 0)", "undefined");
