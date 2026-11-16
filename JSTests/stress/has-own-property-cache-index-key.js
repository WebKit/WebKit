function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function hasOwn(object, key) {
    return object.hasOwnProperty(key);
}
noInline(hasOwn);

for (var i = 0; i < testLoopCount; ++i) {
    var object = { a: 1, b: 2 };
    shouldBe(hasOwn(object, '0'), false);
    shouldBe(hasOwn(object, '1'), false);
    shouldBe(hasOwn(object, 'a'), true);
    if (i % 3 === 0) {
        object[0] = 42;
        shouldBe(hasOwn(object, '0'), true);
        shouldBe(hasOwn(object, '1'), false);
    } else if (i % 3 === 1) {
        Object.defineProperty(object, '1', { get() { return 1; }, configurable: true });
        shouldBe(hasOwn(object, '1'), true);
        shouldBe(hasOwn(object, '0'), false);
    }
    var other = { a: 1, b: 2 };
    shouldBe(hasOwn(other, '0'), false);
    shouldBe(hasOwn([1], '0'), true);
    shouldBe(hasOwn(new String('x'), '0'), true);
    shouldBe(hasOwn(new Uint8Array(1), '0'), true);
}
