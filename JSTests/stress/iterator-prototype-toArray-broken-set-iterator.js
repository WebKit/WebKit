function test(setIter) {
    return Iterator.prototype.toArray.call(setIter);
}

function shouldBe(a, b) {
    if (a !== b) {
        throw new Error(`Expected ${b} but got ${a}`);
    }
}

{
    const set = new Set([1, 2, 3, 4, 5]);
    const setIterator = set.values();
    let count = 0;
    setIterator.next = () => {
        if (count++ > 2)
            return { value: 1, done: true };
        return { value: 1, done: false };
    };
    const arr = test(setIterator);
    shouldBe(arr.length, 3);
    shouldBe(arr[0], 1);
    shouldBe(arr[1], 1);
    shouldBe(arr[2], 1);
}

{
    const set = new Set([1, 2, 3, 4, 5]);
    const setIteratorPrototype = Object.getPrototypeOf(new Set().values());
    let count = 0;
    // break the watchpoint
    setIteratorPrototype.next = () => {
        if (count++ > 2)
            return { value: 1, done: true };
        return { value: 1, done: false };
    };
    const arr = test(set.values());
    shouldBe(arr.length, 3);
    shouldBe(arr[0], 1);
    shouldBe(arr[1], 1);
    shouldBe(arr[2], 1);
}
