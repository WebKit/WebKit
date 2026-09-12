function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test() {
    return __proto__;
}

noInline(test);

shouldBe(this, undefined);
shouldBe(__proto__, globalThis.__proto__);

var result = test();
shouldBe(result, globalThis.__proto__);

// Make sure __proto__ is consistently the same value, and that it doesn't change after JIT compilation.
for (var i = 0; i < 2e6; ++i) {
    result = test();
    shouldBe(result, globalThis.__proto__);
}