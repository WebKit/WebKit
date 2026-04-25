function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function mapSize(m) {
    return m.size;
}
noInline(mapSize);

for (var i = 0; i < testLoopCount; ++i) {
    var m = new Map();
    shouldBe(mapSize(m), 0);

    m.set(1, 'a');
    m.set(2, 'b');
    m.set(3, 'c');
    shouldBe(mapSize(m), 3);

    m.delete(2);
    shouldBe(mapSize(m), 2);

    m.set(2, 'x');
    m.set(4, 'd');
    shouldBe(mapSize(m), 4);

    m.clear();
    shouldBe(mapSize(m), 0);
}
