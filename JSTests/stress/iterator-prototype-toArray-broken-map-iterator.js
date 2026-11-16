function test(mapIterator) {
    return Iterator.prototype.toArray.call(mapIterator);
}

function shouldBe(a, b) {
    if (a !== b) {
        throw new Error(`Expected ${b} but got ${a}`);
    }
}

function shouldThrow(fn, errorMessage) {
    let thrown = false;
    try {
        fn();
    } catch (e) {
        thrown = true;
        if (errorMessage && e.message !== errorMessage) {
            throw new Error(`Expected error message "${errorMessage}" but got "${e.message}"`);
        }
    }
    if (!thrown) {
        throw new Error('Expected to throw but did not');
    }
}

// Test with broken iterator (modified next method)
{
    const map = new Map([[1, 10], [2, 20], [3, 30]]);
    const mapIterator = map.values();
    mapIterator.next = function() {
        throw new Error('Custom error');
    };
    
    shouldThrow(function() {
        test(mapIterator);
    }, 'Custom error');
}

// Test with broken iterator (modified prototype)
{
    const map = new Map([[1, 10], [2, 20]]);
    const mapIterator = map.values();
    const oldProto = Object.getPrototypeOf(mapIterator);
    const newProto = Object.create(oldProto);
    newProto.next = function() {
        throw new Error('Proto error');
    };
    Object.setPrototypeOf(mapIterator, newProto);
    
    shouldThrow(function() {
        test(mapIterator);
    }, 'Proto error');
}

// Test normal case after broken iterator tests
{
    const map = new Map([[1, 10], [2, 20]]);
    const arr = test(map.values());
    shouldBe(arr.length, 2);
    shouldBe(arr[0], 10);
    shouldBe(arr[1], 20);
}
