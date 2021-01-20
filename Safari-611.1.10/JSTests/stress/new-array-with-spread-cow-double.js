function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i) {
        try {
            shouldBe(actual[i], expected[i]);
        } catch(e) {
            print(JSON.stringify(actual));
            throw e;
        }
    }
}

function test(flags)
{
    var array = [0.1, 0.2, 0.3];
    if (flags)
        OSRExit();
    var cloned = [...array];
    return cloned
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    shouldBeArray(test(false), [0.1, 0.2, 0.3]);
shouldBeArray(test(true), [0.1, 0.2, 0.3]);
