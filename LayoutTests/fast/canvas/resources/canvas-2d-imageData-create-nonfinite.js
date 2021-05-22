description("Test the argument bounds of canvas createImageData.");

var canvas = document.getElementById('canvas');
var ctx = canvas.getContext('2d');

shouldThrow("ctx.createImageData(Infinity, Infinity)", '"TypeError: Value Infinity is outside the range [-2147483648, 2147483647]"');
shouldThrow("ctx.createImageData(Infinity, 10)", '"TypeError: Value Infinity is outside the range [-2147483648, 2147483647]"');
shouldThrow("ctx.createImageData(-Infinity, 10)", '"TypeError: Value -Infinity is outside the range [-2147483648, 2147483647]"');
shouldThrow("ctx.createImageData(10, Infinity)", '"TypeError: Value Infinity is outside the range [-2147483648, 2147483647]"');
shouldThrow("ctx.createImageData(10, -Infinity)", '"TypeError: Value -Infinity is outside the range [-2147483648, 2147483647]"');
shouldThrow("ctx.createImageData(NaN, 10)", '"TypeError: Value NaN is outside the range [-2147483648, 2147483647]"');
shouldThrow("ctx.createImageData(10, NaN)", '"TypeError: Value NaN is outside the range [-2147483648, 2147483647]"');
