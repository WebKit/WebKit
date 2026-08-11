function test(setIterator, cb) {
    return Iterator.prototype.forEach.call(setIterator, cb);
}

function shouldBe(a, b) {
    if (a !== b) {
        throw new Error(`Expected ${b} but got ${a}`);
    }
}

{
    const set = new Set([1, 2, 3, 4, 5]);
    let called = 0;
    const values = [];
    const indices = [];
    const cb = (v, i) => {
        called++;
        values.push(v);
        indices.push(i);
    };
    test(set.values(), cb);

    shouldBe(values.length, 5);
    shouldBe(values[0], 1);
    shouldBe(values[1], 2);
    shouldBe(values[2], 3);
    shouldBe(values[3], 4);
    shouldBe(values[4], 5);

    shouldBe(indices.length, 5);
    shouldBe(indices[0], 0);
    shouldBe(indices[1], 1);
    shouldBe(indices[2], 2);
    shouldBe(indices[3], 3);
    shouldBe(indices[4], 4);

    shouldBe(called, 5);
}
