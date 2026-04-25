function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(a, b, c, d, e) {
    return a + b + c + d + e;
}

function ok()
{
    "use strict";
    globalThis.escape = arguments;
    return test.apply(undefined, arguments);
}
noInline(ok);

for (var i = 0; i < 1e4; ++i)
    shouldBe(ok(1, 2, 3, 4, 5), 15);
