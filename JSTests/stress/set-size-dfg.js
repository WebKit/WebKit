function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function setSize(s) {
    return s.size;
}
noInline(setSize);

for (var i = 0; i < testLoopCount; ++i) {
    var s = new Set();
    shouldBe(setSize(s), 0);

    s.add(1);
    s.add(2);
    s.add(3);
    shouldBe(setSize(s), 3);

    s.delete(2);
    shouldBe(setSize(s), 2);

    s.add(2);
    s.add(4);
    shouldBe(setSize(s), 4);

    s.clear();
    shouldBe(setSize(s), 0);
}
