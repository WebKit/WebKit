description("Test the behavior of CanvasRenderingContext2D.strokeText() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var NotEnoughArguments = "TypeError: Not enough arguments";

shouldThrow("ctx.strokeText()", "NotEnoughArguments");
shouldThrow("ctx.strokeText('moo')", "NotEnoughArguments");
shouldThrow("ctx.strokeText('moo',0)", "NotEnoughArguments");
shouldBe("ctx.strokeText('moo',0,0)", "undefined");
shouldBe("ctx.strokeText('moo',0,0,0)", "undefined");
shouldBe("ctx.strokeText('moo',0,0,0,0)", "undefined");
