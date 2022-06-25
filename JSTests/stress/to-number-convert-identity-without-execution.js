function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function test(x, y)
{
    if (x)
        return Number(y);
    return y;
}
noInline(test);

// Converted to Identity, but since Number is handled by inlining, it emits ForceOSRExit.
// So converted Identity is never executed.
for (var i = 0; i < 1e4; ++i)
    shouldBe(test(false, 41), 41);


for (var i = 0; i < 1e4; ++i)
    shouldBe(test(true, 41), 41);
var object = { valueOf() { return 41; } };
for (var i = 0; i < 1e4; ++i)
    shouldBe(test(true, object), 41);
for (var i = 0; i < 1e4; ++i)
    shouldBe(test(true, { valueOf() { return 42.195; } }), 42.195);
