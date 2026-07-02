function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

globalThis.foo = {};
loadString('foo[4294967296]={"a":1};');
shouldBe(JSON.stringify(foo["4294967296"]), '{"a":1}');
shouldBe(foo[0], undefined);

globalThis.bar = {};
loadString('bar[2147483648]=5;');
shouldBe(bar["2147483648"], 5);
shouldBe(bar[0], undefined);

globalThis.baz = [];
loadString('baz[0]=1;baz[1]=2;');
shouldBe(baz[0], 1);
shouldBe(baz[1], 2);
shouldBe(baz.length, 2);
