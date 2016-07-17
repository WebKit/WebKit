description("Test the handling of invalid arguments in canvas getImageData().");

ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.getImageData(NaN, 10, 10, 10)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.getImageData(10, NaN, 10, 10)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.getImageData(10, 10, NaN, 10)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.getImageData(10, 10, 10, NaN)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.getImageData(Infinity, 10, 10, 10)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.getImageData(10, Infinity, 10, 10)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.getImageData(10, 10, Infinity, 10)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.getImageData(10, 10, 10, Infinity)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.getImageData(undefined, 10, 10, 10)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.getImageData(10, undefined, 10, 10)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.getImageData(10, 10, undefined, 10)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.getImageData(10, 10, 10, undefined)", '"TypeError: The provided value is non-finite"');
shouldThrow("ctx.getImageData(10, 10, 0, 10)", '"IndexSizeError (DOM Exception 1): The index is not in the allowed range."');
shouldThrow("ctx.getImageData(10, 10, 10, 0)", '"IndexSizeError (DOM Exception 1): The index is not in the allowed range."');
