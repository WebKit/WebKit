description(
'This test checks createLinearGradient with infinite values'
);

var ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.createLinearGradient(0, 0, 100, NaN)", "'TypeError: Type error'");
shouldThrow("ctx.createLinearGradient(0, 0, 100, Infinity)", "'TypeError: Type error'");
shouldThrow("ctx.createLinearGradient(0, 0, 100, -Infinity)", "'TypeError: Type error'");
shouldThrow("ctx.createLinearGradient(0, 0, NaN, 100)", "'TypeError: Type error'");
shouldThrow("ctx.createLinearGradient(0, 0, Infinity, 100)", "'TypeError: Type error'");
shouldThrow("ctx.createLinearGradient(0, 0, -Infinity, 100)", "'TypeError: Type error'");
shouldThrow("ctx.createLinearGradient(0, NaN, 100, 100)", "'TypeError: Type error'");
shouldThrow("ctx.createLinearGradient(0, Infinity, 100, 100)", "'TypeError: Type error'");
shouldThrow("ctx.createLinearGradient(0, -Infinity, 100, 100)", "'TypeError: Type error'");
shouldThrow("ctx.createLinearGradient(NaN, 0, 100, 100)", "'TypeError: Type error'");
shouldThrow("ctx.createLinearGradient(Infinity, 0, 100, 100)", "'TypeError: Type error'");
shouldThrow("ctx.createLinearGradient(-Infinity, 0, 100, 100)", "'TypeError: Type error'");
