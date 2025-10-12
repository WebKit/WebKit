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

// Test AggregateError constructor with Map values
{
    const map = new Map([['error1', 'Error 1'], ['error2', 'Error 2'], ['error3', 'Error 3']]);
    const aggregateError = new AggregateError(map.values(), 'Multiple errors');
    shouldBe(aggregateError.errors.length, 3);
    shouldBe(aggregateError.errors[0], 'Error 1');
    shouldBe(aggregateError.errors[1], 'Error 2');
    shouldBe(aggregateError.errors[2], 'Error 3');
    shouldBe(aggregateError.message, 'Multiple errors');
}

// Test with broken Map Symbol.iterator
{
    const map = new Map([['key1', 'value1'], ['key2', 'value2']]);
    const oldIterator = map[Symbol.iterator];
    map[Symbol.iterator] = function() {
        throw new Error('Custom iterator error');
    };
    
    // Should still work because it uses fast path for map.values()
    const aggregateError = new AggregateError(map.values(), 'Test message');
    shouldBe(aggregateError.errors.length, 2);
    shouldBe(aggregateError.errors[0], 'value1');
    shouldBe(aggregateError.errors[1], 'value2');
    
    map[Symbol.iterator] = oldIterator;
}

// Test with broken Map Iterator next method
{
    const map = new Map([['key1', 'value1'], ['key2', 'value2']]);
    const mapIterator = map.values();
    mapIterator.next = function() {
        throw new Error('Iterator next error');
    };
    
    shouldThrow(function() {
        new AggregateError(mapIterator, 'Test message');
    }, 'Iterator next error');
}

// Test normal case
{
    const map = new Map([[1, 'a'], [2, 'b'], [3, 'c']]);
    const aggregateError = new AggregateError(map.keys(), 'Test');
    shouldBe(aggregateError.errors.length, 3);
    shouldBe(aggregateError.errors[0], 1);
    shouldBe(aggregateError.errors[1], 2);
    shouldBe(aggregateError.errors[2], 3);
}
