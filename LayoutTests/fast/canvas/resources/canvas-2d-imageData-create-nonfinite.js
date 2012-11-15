description("Test the argument bounds of canvas createImageData.");

var canvas = document.getElementById('canvas');
var ctx = canvas.getContext('2d');

shouldThrow("ctx.createImageData(Infinity, Infinity)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.createImageData(Infinity, 10)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.createImageData(-Infinity, 10)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.createImageData(10, Infinity)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.createImageData(10, -Infinity)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.createImageData(NaN, 10)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.createImageData(10, NaN)", '"Error: NotSupportedError: DOM Exception 9"');
