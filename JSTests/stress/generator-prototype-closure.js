function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test() {
    function* gen()
    {
    }
    var originalPrototype = gen.prototype;

    var g = gen();
    shouldBe(g.__proto__, gen.prototype);
    gen.prototype = {};
    var g = gen();
    shouldBe(g.__proto__, gen.prototype);
    shouldBe(g.__proto__ !== originalPrototype, true);
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test();
