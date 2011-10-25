description("Test the argument bounds of canvas createImageData.");

var canvas = document.getElementById('canvas');
var ctx = canvas.getContext('2d');

shouldThrow("ctx.createImageData(Infinity, Infinity)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.createImageData(Infinity, 10)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.createImageData(-Infinity, 10)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.createImageData(10, Infinity)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.createImageData(10, -Infinity)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.createImageData(NaN, 10)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.createImageData(10, NaN)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
