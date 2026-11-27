//@ defaultRun; runDefault("--useDFGJIT=false")
function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("bad value: " + String(actual) + " expected " + String(expected));
}

function test(flag, a, b) {
    var r = 0;
    for (var i = 0; i < 100; ++i) {
        if (flag)
            r = [a + b, a - b, a * b];
    }
    return r;
}
noInline(test);

for (var i = 0; i < testLoopCount / 5; ++i)
    shouldBe(test(false, 1, 2), 0);

var cases = [
    [1, 2, [3, -1, 2]],
    [1.5, 2, [3.5, -0.5, 3]],
    [0x7fffffff, 1, [2147483648, 2147483646, 2147483647]],
    [-0, 0, [0, -0, -0]],
    ["a", 1, ["a1", NaN, NaN]],
    [2n, 3n, [5n, -1n, 6n]],
    [{ valueOf() { return 4; } }, 2, [6, 2, 8]],
];
for (var i = 0; i < testLoopCount / 50; ++i) {
    for (var [a, b, expected] of cases) {
        var result = test(true, a, b);
        for (var j = 0; j < 3; ++j)
            shouldBe(result[j], expected[j]);
    }
}
