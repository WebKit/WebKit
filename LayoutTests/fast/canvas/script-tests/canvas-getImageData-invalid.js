description("Test the handling of invalid arguments in canvas getImageData().");

ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.getImageData(NaN, 10, 10, 10)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, NaN, 10, 10)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, NaN, 10)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, 10, NaN)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.getImageData(Infinity, 10, 10, 10)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, Infinity, 10, 10)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, Infinity, 10)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, 10, Infinity)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.getImageData(undefined, 10, 10, 10)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, undefined, 10, 10)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, undefined, 10)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, 10, undefined)", '"Error: NotSupportedError: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, 0, 10)", '"Error: IndexSizeError: DOM Exception 1"');
shouldThrow("ctx.getImageData(10, 10, 10, 0)", '"Error: IndexSizeError: DOM Exception 1"');
