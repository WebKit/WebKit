description("Test the argument bounds of canvas createImageData.");

var canvas = document.getElementById('canvas');
var ctx = canvas.getContext('2d');

shouldThrow("ctx.createImageData(Infinity, Infinity)", '"TypeError: Type error"');
shouldThrow("ctx.createImageData(Infinity, 10)", '"TypeError: Type error"');
shouldThrow("ctx.createImageData(-Infinity, 10)", '"TypeError: Type error"');
shouldThrow("ctx.createImageData(10, Infinity)", '"TypeError: Type error"');
shouldThrow("ctx.createImageData(10, -Infinity)", '"TypeError: Type error"');
shouldThrow("ctx.createImageData(NaN, 10)", '"TypeError: Type error"');
shouldThrow("ctx.createImageData(10, NaN)", '"TypeError: Type error"');
