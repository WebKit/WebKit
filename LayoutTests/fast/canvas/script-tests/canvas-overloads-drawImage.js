description("Test the behavior of CanvasRenderingContext2D.drawImage() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var TypeError = "TypeError: Type error";
var TypeErrorNotEnoughArguments = "TypeError: Not enough arguments";

var imageElement = document.createElement("img");
shouldThrow("ctx.drawImage()", "TypeErrorNotEnoughArguments");
shouldThrow("ctx.drawImage(imageElement)", "TypeErrorNotEnoughArguments");
shouldThrow("ctx.drawImage(imageElement, 0)", "TypeErrorNotEnoughArguments");
shouldBe("ctx.drawImage(imageElement, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0)", "TypeError");
shouldBe("ctx.drawImage(imageElement, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldBe("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "TypeError");

var canvasElement = document.createElement("canvas");
shouldThrow("ctx.drawImage(canvasElement)", "TypeErrorNotEnoughArguments");
shouldThrow("ctx.drawImage(canvasElement, 0)", "TypeErrorNotEnoughArguments");
shouldBe("ctx.drawImage(canvasElement, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0)", "TypeError");
shouldBe("ctx.drawImage(canvasElement, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0, 0)", "'Error: IndexSizeError: DOM Exception 1'");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
