description(
'This test checks createRadialGradient with infinite values'
);

var ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, 0, NaN)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, 0, Infinity)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, 0, -Infinity)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, NaN, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, Infinity, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, -Infinity, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, NaN, 0, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, Infinity, 0, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, -Infinity, 0, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, 0, NaN, 0, 0, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, 0, Infinity, 0, 0, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, 0, -Infinity, 0, 0, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, NaN, 100, 0, 0, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, Infinity, 100, 0, 0, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(0, -Infinity, 100, 0, 0, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(NaN, 0, 100, 0, 0, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(Infinity, 0, 100, 0, 0, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createRadialGradient(-Infinity, 0, 100, 0, 0, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");

var successfullyParsed = true;
