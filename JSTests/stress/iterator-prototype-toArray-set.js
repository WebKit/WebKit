function test(setIterator) {
    return Iterator.prototype.toArray.call(setIterator);
}

function shouldBe(a, b) {
    if (a !== b) {
        throw new Error(`Expected ${b} but got ${a}`);
    }
}

{
    const set = new Set([1, 2, 3, 4, 5]);
    const arr = test(set.values());
    shouldBe(arr.length, 5);
    shouldBe(arr[0], 1);
    shouldBe(arr[1], 2);
    shouldBe(arr[2], 3);
    shouldBe(arr[3], 4);
    shouldBe(arr[4], 5);
}

{
    const set = new Set([1, 2, 3, 4, 5]);
    const setIter = set.values();
    setIter.next();
    const arr = test(setIter);
    shouldBe(arr.length, 4);
    shouldBe(arr[0], 2);
    shouldBe(arr[1], 3);
    shouldBe(arr[2], 4);
    shouldBe(arr[3], 5);
}
