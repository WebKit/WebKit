description("Test the argument bounds of canvas createImageData.");

var canvas = document.getElementById('canvas');
var ctx = canvas.getContext('2d');

shouldThrow("ctx.createImageData(Infinity, Infinity)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.createImageData(Infinity, 10)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.createImageData(-Infinity, 10)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.createImageData(10, Infinity)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.createImageData(10, -Infinity)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.createImageData(NaN, 10)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.createImageData(10, NaN)", '"TypeError: The provided value is non-finite"');
