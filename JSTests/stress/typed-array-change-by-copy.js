function shouldBe(actual, expected) {
    if (!shallowEqual(actual, expected)){
        throw new Error(`expected ${expected} but got ${actual}`);
    }
}

function shouldThrow(func, errorType, message) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
    if (message !== undefined) {
        if (Object.prototype.toString.call(message) === '[object RegExp]') {
            if (!message.test(String(error)))
                throw new Error(`expected '${String(error)}' to match ${message}!`);
        } else {
            if (String(error) !== message)
                throw new Error(`expected ${String(error)} but got ${message}`);
        }
    }
}

function shallowEqual(a, b) {
    if (a.length !== b.length)
        return false;
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            return false;
    }
    return true;
}

var sequence = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);

// TypedArray.prototype.toReversed()
{
    var reversedTypedArray = sequence.toReversed();
    shouldBe(reversedTypedArray, [10, 9, 8, 7, 6, 5, 4, 3, 2, 1]);
}

// TypedArray.prototype.toSorted()
{
    var unsortedTypedArray = new Uint8Array([9, 2, 3, 4, 56, 12, 0]);
    let sortedTypedArray = unsortedTypedArray.toSorted();
    shouldBe(sortedTypedArray, [0, 2, 3, 4, 9, 12, 56]);

    // Non callable comparator.
    var nonCallableComparator = "a";
    shouldThrow(() => unsortedTypedArray.toSorted(nonCallableComparator), TypeError);
}

// TypedArray.prototype.with()
{
    var withTypedArray = sequence.with(3, 0);
    shouldBe(withTypedArray, [1, 2, 3, 0, 5, 6, 7, 8, 9, 10]);

    // Index missing
    withTypedArray = sequence.with();
    shouldBe(withTypedArray, [0, 2, 3, 4, 5, 6, 7, 8, 9, 10]);

    // Index undefined
    withTypedArray = sequence.with(undefined, 0);
    shouldBe(withTypedArray, [0, 2, 3, 4, 5, 6, 7, 8, 9, 10]);

    // Invalid index.
    shouldThrow(() => sequence.with(-11, 0), RangeError);

    // Detached buffer.
    transferArrayBuffer(sequence.buffer);
    shouldThrow(() => sequence.with(0, 0), TypeError);
}


