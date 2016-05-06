description(
'This test checks createRadialGradient with infinite values'
);

var ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, 0, NaN)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, 0, Infinity)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, 0, -Infinity)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, NaN, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, Infinity, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, -Infinity, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, NaN, 0, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, Infinity, 0, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, -Infinity, 0, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, 0, NaN, 0, 0, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, 0, Infinity, 0, 0, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, 0, -Infinity, 0, 0, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, NaN, 100, 0, 0, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, Infinity, 100, 0, 0, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(0, -Infinity, 100, 0, 0, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(NaN, 0, 100, 0, 0, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(Infinity, 0, 100, 0, 0, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createRadialGradient(-Infinity, 0, 100, 0, 0, 100)", "'TypeError: The provided value is non-finite'");
