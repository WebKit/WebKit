function shouldBe(actual, expected, message) {
    if (!Object.is(actual, expected))
        throw new Error(message + ": expected " + expected + " but got " + actual);
}

function pow(x, y) {
    return Math.pow(x, y);
}
noInline(pow);

var cases = [
    [1, 0.01666666753590107, 1],
    [1, 0.5, 1],
    [1, -0.5, 1],
    [1, 2.5, 1],
    [1, -2.5, 1],
    [1, 0, 1],
    [1, -0, 1],
    [1, 1e300, 1],
    [1, -1e300, 1],
    [1, Number.MIN_VALUE, 1],
    [1, NaN, NaN],
    [1, Infinity, NaN],
    [1, -Infinity, NaN],
    [-1, 0.5, NaN],
    [-1, 2.5, NaN],
    [-1, Infinity, NaN],
    [-1, -Infinity, NaN],
    [-1, NaN, NaN],
    [1.0000000000000002, 1e300, Infinity],
    [0.9999999999999999, 1e300, 0],
    [2, 0.5, Math.SQRT2],
    [4, -0.5, 0.5],
    [0, 0.5, 0],
    [-0, 0.5, 0],
    [Infinity, 0.5, Infinity],
    [Infinity, -0.5, 0],
];

for (var i = 0; i < testLoopCount; ++i) {
    for (var [x, y, expected] of cases)
        shouldBe(pow(x, y), expected, "Math.pow(" + x + ", " + y + ")");
}
