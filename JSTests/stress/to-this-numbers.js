function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

Number.prototype.toThis = function toThis()
{
    'use strict';
    return this;
};
noInline(Number.prototype.toThis);

for (var i = 0; i < 1e4; ++i) {
    shouldBe((0.1).toThis(), 0.1);
    shouldBe((42).toThis(), 42);
    shouldBe((1024 * 1024 * 1024 * 1024).toThis(), (1024 * 1024 * 1024 * 1024));
}
