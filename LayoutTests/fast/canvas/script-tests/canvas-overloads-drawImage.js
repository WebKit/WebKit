description("Test the behavior of CanvasRenderingContext2D.drawImage() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var TypeError = "TypeError: Type error";

var imageElement = document.createElement("img");
shouldThrow("ctx.drawImage()", "TypeError");
shouldThrow("ctx.drawImage(imageElement)", "TypeError");
shouldThrow("ctx.drawImage(imageElement, 0)", "TypeError");
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
shouldThrow("ctx.drawImage(canvasElement)", "TypeError");
shouldThrow("ctx.drawImage(canvasElement, 0)", "TypeError");
shouldBe("ctx.drawImage(canvasElement, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0)", "TypeError");
shouldBe("ctx.drawImage(canvasElement, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0, 0)", "'Error: INDEX_SIZE_ERR: DOM Exception 1'");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
