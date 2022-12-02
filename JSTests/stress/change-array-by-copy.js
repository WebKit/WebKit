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

var sequence = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

// Array.prototype.toReversed()
{
    let reversedSequence = sequence.toReversed();
    shouldBe(reversedSequence, [10, 9, 8, 7, 6, 5, 4, 3, 2, 1]);

    // |this| = null.
    shouldThrow(() => Array.prototype.toSpliced.toReversed(null, 0, 5), TypeError);
}

// Array.prototype.toSorted()
{
    var unsortedSequence = [9, 2, 3, 4, 56, 12, 0];
    var sortedSequence = unsortedSequence.toSorted();
    shouldBe(sortedSequence, [0, 12, 2, 3, 4, 56, 9]);

    // |this| = null.
    shouldThrow(() => Array.prototype.toSorted.call(null), TypeError);

    // Non callable comparator.
    var nonCallableComparator = "a";
    shouldThrow(() => unsortedSequence.toSorted(nonCallableComparator), TypeError);
}

// Array.prototype.toSpliced()
{
    var splicedSequence = sequence.toSpliced(3, 5);
    shouldBe(splicedSequence, [1, 2, 3, 9, 10]);

    // Start missing.
    splicedSequence = sequence.toSpliced();
    shouldBe(splicedSequence, sequence);

    // Delete count missing.
    splicedSequence = sequence.toSpliced(3);
    shouldBe(splicedSequence, [1, 2, 3]);

    // Start undefined.
    splicedSequence = sequence.toSpliced(undefined);
    shouldBe(splicedSequence, []);

    // Delete count undefined.
    splicedSequence = sequence.toSpliced(1, undefined);
    shouldBe(splicedSequence, sequence);

    // Insertion
    splicedSequence = sequence.toSpliced(3, 5, 11, 12, 13);
    shouldBe(splicedSequence, [1, 2, 3, 11, 12, 13, 9, 10]);

    // |this| = null.
    shouldThrow(() => Array.prototype.toSpliced.call(null, 0, 5), TypeError);

    // Length >= 2^53 - 1.
    var arrayLike = {};
    arrayLike.length = 2 ** 53 - 1;
    shouldThrow(() => Array.prototype.toSpliced.call(arrayLike, 0, 0, null), TypeError);
}

//Array.prototype.with()
{
    var withSequence = sequence.with(3, "a");
    shouldBe(withSequence, [1, 2, 3, "a", 5, 6, 7, 8, 9, 10]);

    // Index missing.
    withSequence = sequence.with();
    shouldBe(withSequence, [undefined, 2, 3, 4, 5, 6, 7, 8, 9, 10]);

    // Index undefined.
    withSequence = sequence.with(undefined, "a");
    shouldBe(withSequence, ["a", 2, 3, 4, 5, 6, 7, 8, 9, 10]);

    // Invalid index.
    shouldThrow(() => sequence.with(-11, "a"), RangeError);

    // |this| = null.
    shouldThrow(() => Array.prototype.toReversed.with(null, 5, "a"), TypeError);
}
