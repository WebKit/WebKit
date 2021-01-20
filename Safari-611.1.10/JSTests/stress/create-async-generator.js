function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test() {
    async function* generator() {
        return this;
    }

    var g = generator();
    shouldBe(g.__proto__, generator.prototype);
    return g;
}

for (var i = 0; i < 1e6; ++i) {
    var g1 = test();
    var g2 = test();
    shouldBe(g1.__proto__ != g2.__proto__, true);
}
