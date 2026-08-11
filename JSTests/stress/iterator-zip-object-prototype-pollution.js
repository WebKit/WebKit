//@ requireOptions("--useJointIteration=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function makeIterator(values, log) {
    var index = 0;
    return {
        next() {
            if (index < values.length)
                return { value: values[index++], done: false };
            return { value: undefined, done: true };
        },
        return() {
            log.push("return");
            return { value: undefined, done: true };
        },
        [Symbol.iterator]() { return this; }
    };
}

// GetOptionsObject must produce a null-prototype object when options is undefined,
// so "mode" and "padding" lookups do not read through Object.prototype.
{
    Object.defineProperty(Object.prototype, "mode", { value: "strict", configurable: true });
    Object.defineProperty(Object.prototype, "padding", { value: null, configurable: true });
    try {
        shouldBe(JSON.stringify(Array.from(Iterator.zip([[1, 2, 3], [4, 5]]))), "[[1,4],[2,5]]");
        shouldBe(JSON.stringify(Array.from(Iterator.zipKeyed({ a: [1, 2, 3], b: [4, 5] }))), `[{"a":1,"b":4},{"a":2,"b":5}]`);
    } finally {
        delete Object.prototype.mode;
        delete Object.prototype.padding;
    }
}

// The synthetic underlying iterator's return() must be installed on a null-prototype object.
// A non-writable Object.prototype.return must not make zip creation throw.
{
    Object.defineProperty(Object.prototype, "return", { value: 42, writable: false, configurable: true });
    try {
        var log = [];
        shouldBe(JSON.stringify(Array.from(Iterator.zip([makeIterator([1], log), makeIterator([2], log)]))), "[[1,2]]");
        shouldBe(JSON.stringify(Array.from(Iterator.zipKeyed({ a: makeIterator([1], log), b: makeIterator([2], log) }))), `[{"a":1,"b":2}]`);
    } finally {
        delete Object.prototype.return;
    }
}

// An Object.prototype.return accessor must not swallow the closure: closing the zip result
// closes the underlying iterators.
{
    Object.defineProperty(Object.prototype, "return", { get() { return undefined; }, set(v) { }, configurable: true });
    try {
        var log = [];
        var zipped = Iterator.zip([makeIterator([1, 2, 3], log), makeIterator([4, 5, 6], log)]);
        zipped.next();
        zipped.return();
        shouldBe(log.length, 2);

        log = [];
        var zippedKeyed = Iterator.zipKeyed({ a: makeIterator([1, 2, 3], log), b: makeIterator([4, 5, 6], log) });
        zippedKeyed.next();
        zippedKeyed.return();
        shouldBe(log.length, 2);
    } finally {
        delete Object.prototype.return;
    }
}
