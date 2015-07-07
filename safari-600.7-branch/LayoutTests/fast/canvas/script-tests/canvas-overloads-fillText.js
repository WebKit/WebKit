description("Test the behavior of CanvasRenderingContext2D.fillText() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var NotEnoughArguments = "TypeError: Not enough arguments";
var TypeError = "TypeError: Type error";

shouldThrow("ctx.fillText()", "NotEnoughArguments");
shouldThrow("ctx.fillText('moo')", "NotEnoughArguments");
shouldThrow("ctx.fillText('moo',0)", "NotEnoughArguments");
shouldBe("ctx.fillText('moo',0,0)", "undefined");
shouldBe("ctx.fillText('moo',0,0,0)", "undefined");
shouldBe("ctx.fillText('moo',0,0,0,0)", "undefined");
