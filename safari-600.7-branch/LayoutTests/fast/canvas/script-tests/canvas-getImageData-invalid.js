description("Test the handling of invalid arguments in canvas getImageData().");

ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.getImageData(NaN, 10, 10, 10)", '"TypeError: Type error"');
shouldThrow("ctx.getImageData(10, NaN, 10, 10)", '"TypeError: Type error"');
shouldThrow("ctx.getImageData(10, 10, NaN, 10)", '"TypeError: Type error"');
shouldThrow("ctx.getImageData(10, 10, 10, NaN)", '"TypeError: Type error"');
shouldThrow("ctx.getImageData(Infinity, 10, 10, 10)", '"TypeError: Type error"');
shouldThrow("ctx.getImageData(10, Infinity, 10, 10)", '"TypeError: Type error"');
shouldThrow("ctx.getImageData(10, 10, Infinity, 10)", '"TypeError: Type error"');
shouldThrow("ctx.getImageData(10, 10, 10, Infinity)", '"TypeError: Type error"');
shouldThrow("ctx.getImageData(undefined, 10, 10, 10)", '"TypeError: Type error"');
shouldThrow("ctx.getImageData(10, undefined, 10, 10)", '"TypeError: Type error"');
shouldThrow("ctx.getImageData(10, 10, undefined, 10)", '"TypeError: Type error"');
shouldThrow("ctx.getImageData(10, 10, 10, undefined)", '"TypeError: Type error"');
shouldThrow("ctx.getImageData(10, 10, 0, 10)", '"Error: IndexSizeError: DOM Exception 1"');
shouldThrow("ctx.getImageData(10, 10, 10, 0)", '"Error: IndexSizeError: DOM Exception 1"');
