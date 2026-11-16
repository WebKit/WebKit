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

var object = { valueOf() { return 41; } };
// Since Number is handled by inlining, it emits ForceOSRExit.
for (var i = 0; i < 1e4; ++i)
    shouldBe(test(false, object), object);

for (var i = 0; i < 1e4; ++i)
    shouldBe(test(true, object), 41);
