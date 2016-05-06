description(
'This test checks createLinearGradient with infinite values'
);

var ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.createLinearGradient(0, 0, 100, NaN)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createLinearGradient(0, 0, 100, Infinity)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createLinearGradient(0, 0, 100, -Infinity)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createLinearGradient(0, 0, NaN, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createLinearGradient(0, 0, Infinity, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createLinearGradient(0, 0, -Infinity, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createLinearGradient(0, NaN, 100, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createLinearGradient(0, Infinity, 100, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createLinearGradient(0, -Infinity, 100, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createLinearGradient(NaN, 0, 100, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createLinearGradient(Infinity, 0, 100, 100)", "'TypeError: The provided value is non-finite'");
shouldThrow("ctx.createLinearGradient(-Infinity, 0, 100, 100)", "'TypeError: The provided value is non-finite'");
