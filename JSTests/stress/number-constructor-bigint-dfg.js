function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function convert(bigInt)
{
    return Number(bigInt);
}
noInline(convert);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(convert(0n), 0);
    shouldBe(convert(0x7fffffffn), 0x7fffffff);
    shouldBe(convert(-0x7fffffffn - 1n), -0x80000000);
}

for (var i = 0; i < 1e4; ++i) {
    shouldBe(convert(0x80000000n), 0x80000000);
    shouldBe(convert(0x7fffffffn + 1n), 0x80000000);
    shouldBe(convert(0x7fffffffn + 2n), 0x80000001);
    shouldBe(convert(-0x7fffffffn - 2n), -0x80000001);
}

for (var i = 0; i < 1e4; ++i) {
    shouldBe(convert(0x20000000000000n), 9007199254740992);
    shouldBe(convert(0x20000000000000n + 1n), 9007199254740992);
    shouldBe(convert(0x20000000000000n + 2n), 9007199254740994);
    shouldBe(convert(0x20000000000000n + 3n), 9007199254740996);
    shouldBe(convert(0x20000000000000n + 4n), 9007199254740996);

    shouldBe(convert(-(0x20000000000000n)), -9007199254740992);
    shouldBe(convert(-(0x20000000000000n + 1n)), -9007199254740992);
    shouldBe(convert(-(0x20000000000000n + 2n)), -9007199254740994);
    shouldBe(convert(-(0x20000000000000n + 3n)), -9007199254740996);
    shouldBe(convert(-(0x20000000000000n + 4n)), -9007199254740996);

    shouldBe(convert(2n ** (1024n - 1n)), 8.98846567431158e+307);
    shouldBe(convert(2n ** (1024n - 1n)), 8.98846567431158e+307);
    shouldBe(convert(0x1fffffffffffffn << 971n), Number.MAX_VALUE);
    shouldBe(convert(0x20000000000000n << 971n), Infinity);
    shouldBe(convert(0x1ffffffffffffffn << 966n), 8.98846567431158e+307);
    shouldBe(convert(0x3fffffffffffffn << 970n), Infinity);
    shouldBe(convert(0x3fffffffffffffn << 969n), 8.98846567431158e+307);

    shouldBe(convert(-(2n ** (1024n - 1n))), -8.98846567431158e+307);
    shouldBe(convert(-(2n ** (1024n - 1n))), -8.98846567431158e+307);
    shouldBe(convert(-0x1fffffffffffffn << 971n), -Number.MAX_VALUE);
    shouldBe(convert(-0x20000000000000n << 971n), -Infinity);
    shouldBe(convert(-0x1ffffffffffffffn << 966n), -8.98846567431158e+307);
    shouldBe(convert(-0x3fffffffffffffn << 970n), -Infinity);
    shouldBe(convert(-0x3fffffffffffffn << 969n), -8.98846567431158e+307);
}
