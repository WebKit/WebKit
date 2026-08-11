function test(mapIterator) {
    return Iterator.prototype.toArray.call(mapIterator);
}

function shouldBe(a, b) {
    if (a !== b) {
        throw new Error(`Expected ${b} but got ${a}`);
    }
}

// Test with values iterator
{
    const map = new Map([[1, 'a'], [2, 'b'], [3, 'c'], [4, 'd'], [5, 'e']]);
    const arr = test(map.values());
    shouldBe(arr.length, 5);
    shouldBe(arr[0], 'a');
    shouldBe(arr[1], 'b');
    shouldBe(arr[2], 'c');
    shouldBe(arr[3], 'd');
    shouldBe(arr[4], 'e');
}

// Test with keys iterator
{
    const map = new Map([[1, 'a'], [2, 'b'], [3, 'c'], [4, 'd'], [5, 'e']]);
    const arr = test(map.keys());
    shouldBe(arr.length, 5);
    shouldBe(arr[0], 1);
    shouldBe(arr[1], 2);
    shouldBe(arr[2], 3);
    shouldBe(arr[3], 4);
    shouldBe(arr[4], 5);
}

// Test with entries iterator
{
    const map = new Map([[1, 'a'], [2, 'b']]);
    const arr = test(map.entries());
    shouldBe(arr.length, 2);
    shouldBe(arr[0].length, 2);
    shouldBe(arr[0][0], 1);
    shouldBe(arr[0][1], 'a');
    shouldBe(arr[1][0], 2);
    shouldBe(arr[1][1], 'b');
}

// Test with partially consumed iterator
{
    const map = new Map([[1, 'a'], [2, 'b'], [3, 'c'], [4, 'd'], [5, 'e']]);
    const mapIter = map.values();
    mapIter.next();
    const arr = test(mapIter);
    shouldBe(arr.length, 4);
    shouldBe(arr[0], 'b');
    shouldBe(arr[1], 'c');
    shouldBe(arr[2], 'd');
    shouldBe(arr[3], 'e');
}
