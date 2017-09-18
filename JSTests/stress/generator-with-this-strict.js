function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function* generator()
{
    'use strict'
    return this;
}

function target()
{
    var gen = generator();
    return gen.next().value;
}
noInline(target);

for (var i = 0; i < 1e6; ++i)
    shouldBe(target(), undefined);
