(function () {
'use strict';

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function ok(value)
{
    return value;
}

function a(b = 42, c = 43, d)
{
    return c + ok(b);
}

for (var i = 0; i < 1e4; ++i) {
    shouldBe(a(), 85);
    shouldBe(a(33), 76);
    shouldBe(a(33, 22), 55);
}
}());
