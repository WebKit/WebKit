function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function test(x) {
    var y = x;
    var z = y * 2;
    if (z) {
        z += y;
        z += y;
        z += y;
    }
    return isFinite(z);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    shouldBe(test(1000000000), true);

// Extend to Doubles.
for (var i = 0; i < 1e4; ++i)
    shouldBe(test(42.195), true);

// Leave ToNumber for objects.
// And this should update the value profiling to accept doubles in ToNumber calls.
var object = { valueOf() { return 42.195; } };
for (var i = 0; i < 1e4; ++i)
    shouldBe(test(object), true);
