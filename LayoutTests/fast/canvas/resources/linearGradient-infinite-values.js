description(
'This test checks createLinearGradient with infinite values'
);

var ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.createLinearGradient(0, 0, 100, NaN)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createLinearGradient(0, 0, 100, Infinity)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createLinearGradient(0, 0, 100, -Infinity)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createLinearGradient(0, 0, NaN, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createLinearGradient(0, 0, Infinity, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createLinearGradient(0, 0, -Infinity, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createLinearGradient(0, NaN, 100, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createLinearGradient(0, Infinity, 100, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createLinearGradient(0, -Infinity, 100, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createLinearGradient(NaN, 0, 100, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createLinearGradient(Infinity, 0, 100, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");
shouldThrow("ctx.createLinearGradient(-Infinity, 0, 100, 100)", "'Error: NOT_SUPPORTED_ERR: DOM Exception 9'");

var successfullyParsed = true;
