function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

var proxy = new Proxy([10, 20, 30], {
    get(target, key, receiver) {
        shouldBe(typeof key, 'string');
        return Reflect.get(target, key, receiver);
    },
    has(target, key) {
        shouldBe(typeof key, 'string');
        return Reflect.has(target, key);
    }
});

function test(index) {
    return proxy[index];
}
noInline(test);

function testIn(index) {
    return index in proxy;
}
noInline(testIn);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(test(i % 3), (i % 3 + 1) * 10);
    shouldBe(test(-1), undefined);
    shouldBe(test(1000), undefined);
    shouldBe(testIn(i % 3), true);
    shouldBe(testIn(5), false);
}
shouldBe(test(1.5), undefined);
shouldBe(test('length'), 3);
shouldBe(testIn(0.5), false);
