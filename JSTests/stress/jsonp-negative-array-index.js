function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

globalThis.foo = {};
loadString('foo[-1]={"a":1};');
shouldBe(JSON.stringify(foo["-1"]), '{"a":1}');
shouldBe(foo[0], undefined);

globalThis.bar = {};
loadString('bar[-2147483648]={"b":2};');
shouldBe(JSON.stringify(bar["-2147483648"]), '{"b":2}');
shouldBe(bar[0], undefined);

globalThis.baz = {};
loadString('baz[-0.5]={"c":3};');
shouldBe(JSON.stringify(baz["-0.5"]), '{"c":3}');
shouldBe(baz[0], undefined);

globalThis.qux = [];
loadString('qux[-0]={"d":4};');
shouldBe(JSON.stringify(qux[0]), '{"d":4}');
shouldBe(qux["-0"], undefined);
