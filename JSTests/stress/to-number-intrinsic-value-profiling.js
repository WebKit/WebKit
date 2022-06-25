function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function test(x) {
    return isFinite(x);
}
noInline(test);

var object = { valueOf() { return 42; } };
for (var i = 0; i < 1e4; ++i)
    shouldBe(test(object), true);

// This should update the value profiling to accept doubles in ToNumber calls.
var object = { valueOf() { return 42.195; } };
for (var i = 0; i < 1e4; ++i)
    shouldBe(test(object), true);
