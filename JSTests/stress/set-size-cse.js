function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function testSizeCSE(s, f) {
    let first = s.size;
    f();
    let second = s.size;
    return { first, second };
}
noInline(testSizeCSE);

for (var i = 0; i < testLoopCount; ++i) {
    var s = new Set([1, 2, 3]);

    // add changes size
    var result = testSizeCSE(s, () => s.add(4));
    shouldBe(result.first, 3);
    shouldBe(result.second, 4);

    // delete changes size
    result = testSizeCSE(s, () => s.delete(1));
    shouldBe(result.first, 4);
    shouldBe(result.second, 3);

    // no side effect (CSE is valid)
    result = testSizeCSE(s, () => {});
    shouldBe(result.first, 3);
    shouldBe(result.second, 3);
}
