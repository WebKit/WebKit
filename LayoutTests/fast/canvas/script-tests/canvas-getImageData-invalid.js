description("Test the handling of invalid arguments in canvas getImageData().");

ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.getImageData(NaN, 10, 10, 10)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, NaN, 10, 10)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, NaN, 10)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, 10, NaN)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.getImageData(Infinity, 10, 10, 10)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, Infinity, 10, 10)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, Infinity, 10)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, 10, Infinity)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.getImageData(undefined, 10, 10, 10)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, undefined, 10, 10)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, undefined, 10)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, 10, undefined)", '"Error: NOT_SUPPORTED_ERR: DOM Exception 9"');
shouldThrow("ctx.getImageData(10, 10, 0, 10)", '"Error: INDEX_SIZE_ERR: DOM Exception 1"');
shouldThrow("ctx.getImageData(10, 10, 10, 0)", '"Error: INDEX_SIZE_ERR: DOM Exception 1"');
