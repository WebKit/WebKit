//@ requireOptions("--useIteratorChunking=1")

function assert(a, text) {
    if (!a)
        throw new Error(`Failed assertion: ${text}`);
}

function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function sameArray(a, b) {
    sameValue(JSON.stringify(a), JSON.stringify(b));
}

function shouldThrow(fn, error, message) {
    try {
        fn();
        throw new Error('Expected to throw, but succeeded');
    } catch (e) {
        if (!(e instanceof error))
            throw new Error(`Expected to throw ${error.name} but got ${e.name}`);
        if (e.message !== message)
            throw new Error(`Expected ${error.name} with '${message}' but got '${e.message}'`);
    }
}

// without second argument

{
    class Iter extends Iterator {
        i = 1;
        next() {
            if (this.i > 5)
                return { value: this.i, done: true }
            return { value: this.i++, done: false }
        }
    }
    const iter = new Iter();
    const result = Array.from(iter.windows(1));
    sameArray(result, [[1], [2], [3], [4], [5]]);
}

{
    const iter = {
        i: 1,
        next() {
            if (this.i > 5)
                return { value: this.i, done: true }
            return { value: this.i++, done: false }
        },
    };
    const result = Array.from(Iterator.prototype.windows.call(iter, 2));
    sameArray(result, [[1, 2], [2, 3], [3, 4], [4, 5]]);
}

{
    let nextGetCount = 0;
    class Iter extends Iterator {
        get next() {
            nextGetCount++;
            let i = 1;
            return function() {
                if (i > 5)
                    return { value: i, done: true }
                return { value: i++, done: false }
            }
        };
    };
    const iter = new Iter();
    sameValue(nextGetCount, 0);
    const result = Array.from(iter.windows(3));
    sameValue(nextGetCount, 1);
    sameArray(result, [[1, 2, 3], [2, 3, 4], [3, 4, 5]]);
}

{
    function* gen() {
        yield 1;
        yield 2;
        yield 3;
        yield 4;
        yield 5;
    }
    const iter = gen();
    const result = Array.from(iter.windows(4));
    sameArray(result, [[1, 2, 3, 4], [2, 3, 4, 5]]);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.windows === Iterator.prototype.windows);
    const result = Array.from(iter.windows(5));
    sameArray(result, [[1, 2, 3, 4, 5]]);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.windows === Iterator.prototype.windows);
    const result = Array.from(iter.windows(6));
    sameArray(result, []);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.windows === Iterator.prototype.windows);
    const windows = iter.windows(2);
    const result1 = windows.next().value;
    sameArray(result1, [1, 2]);
    result1.pop();
    sameArray(result1, [1]);
    const result2 = windows.next().value;
    sameArray(result2, [2, 3]);
    result2.pop();
    sameArray(result2, [2]);
    assert(result1 !== result2);
    const result3 = windows.next().value;
    sameArray(result3, [3, 4]);
    result3.pop();
    sameArray(result3, [3]);
    assert(result2 !== result3);
    const result4 = windows.next().value;
    sameArray(result4, [4, 5]);
    result4.pop();
    sameArray(result4, [4]);
    assert(result3 !== result4);
    assert(windows.next().done);
}

// with second argument "only-full"

{
    class Iter extends Iterator {
        i = 1;
        next() {
            if (this.i > 5)
                return { value: this.i, done: true }
            return { value: this.i++, done: false }
        }
    }
    const iter = new Iter();
    const result = Array.from(iter.windows(1, "only-full"));
    sameArray(result, [[1], [2], [3], [4], [5]]);
}

{
    const iter = {
        i: 1,
        next() {
            if (this.i > 5)
                return { value: this.i, done: true }
            return { value: this.i++, done: false }
        },
    };
    const result = Array.from(Iterator.prototype.windows.call(iter, 2, "only-full"));
    sameArray(result, [[1, 2], [2, 3], [3, 4], [4, 5]]);
}

{
    let nextGetCount = 0;
    class Iter extends Iterator {
        get next() {
            nextGetCount++;
            let i = 1;
            return function() {
                if (i > 5)
                    return { value: i, done: true }
                return { value: i++, done: false }
            }
        };
    };
    const iter = new Iter();
    sameValue(nextGetCount, 0);
    const result = Array.from(iter.windows(3, "only-full"));
    sameValue(nextGetCount, 1);
    sameArray(result, [[1, 2, 3], [2, 3, 4], [3, 4, 5]]);
}

{
    function* gen() {
        yield 1;
        yield 2;
        yield 3;
        yield 4;
        yield 5;
    }
    const iter = gen();
    const result = Array.from(iter.windows(4, "only-full"));
    sameArray(result, [[1, 2, 3, 4], [2, 3, 4, 5]]);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.windows === Iterator.prototype.windows);
    const result = Array.from(iter.windows(5, "only-full"));
    sameArray(result, [[1, 2, 3, 4, 5]]);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.windows === Iterator.prototype.windows);
    const result = Array.from(iter.windows(6, "only-full"));
    sameArray(result, []);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.windows === Iterator.prototype.windows);
    const windows = iter.windows(2, "only-full");
    const result1 = windows.next().value;
    sameArray(result1, [1, 2]);
    result1.pop();
    sameArray(result1, [1]);
    const result2 = windows.next().value;
    sameArray(result2, [2, 3]);
    result2.pop();
    sameArray(result2, [2]);
    assert(result1 !== result2);
    const result3 = windows.next().value;
    sameArray(result3, [3, 4]);
    result3.pop();
    sameArray(result3, [3]);
    assert(result2 !== result3);
    const result4 = windows.next().value;
    sameArray(result4, [4, 5]);
    result4.pop();
    sameArray(result4, [4]);
    assert(result3 !== result4);
    assert(windows.next().done);
}

// with second argument "allow-partial"

{
    class Iter extends Iterator {
        i = 1;
        next() {
            if (this.i > 5)
                return { value: this.i, done: true }
            return { value: this.i++, done: false }
        }
    }
    const iter = new Iter();
    const result = Array.from(iter.windows(1, "allow-partial"));
    sameArray(result, [[1], [2], [3], [4], [5]]);
}

{
    const iter = {
        i: 1,
        next() {
            if (this.i > 5)
                return { value: this.i, done: true }
            return { value: this.i++, done: false }
        },
    };
    const result = Array.from(Iterator.prototype.windows.call(iter, 2, "allow-partial"));
    sameArray(result, [[1, 2], [2, 3], [3, 4], [4, 5]]);
}

{
    let nextGetCount = 0;
    class Iter extends Iterator {
        get next() {
            nextGetCount++;
            let i = 1;
            return function() {
                if (i > 5)
                    return { value: i, done: true }
                return { value: i++, done: false }
            }
        };
    };
    const iter = new Iter();
    sameValue(nextGetCount, 0);
    const result = Array.from(iter.windows(3, "allow-partial"));
    sameValue(nextGetCount, 1);
    sameArray(result, [[1, 2, 3], [2, 3, 4], [3, 4, 5]]);
}

{
    function* gen() {
        yield 1;
        yield 2;
        yield 3;
        yield 4;
        yield 5;
    }
    const iter = gen();
    const result = Array.from(iter.windows(4, "allow-partial"));
    sameArray(result, [[1, 2, 3, 4], [2, 3, 4, 5]]);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.windows === Iterator.prototype.windows);
    const result = Array.from(iter.windows(5, "allow-partial"));
    sameArray(result, [[1, 2, 3, 4, 5]]);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.windows === Iterator.prototype.windows);
    const result = Array.from(iter.windows(6, "allow-partial"));
    sameArray(result, [[1, 2, 3, 4, 5]]);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.windows === Iterator.prototype.windows);
    const windows = iter.windows(2, "allow-partial");
    const result1 = windows.next().value;
    sameArray(result1, [1, 2]);
    result1.pop();
    sameArray(result1, [1]);
    const result2 = windows.next().value;
    sameArray(result2, [2, 3]);
    result2.pop();
    sameArray(result2, [2]);
    assert(result1 !== result2);
    const result3 = windows.next().value;
    sameArray(result3, [3, 4]);
    result3.pop();
    sameArray(result3, [3]);
    assert(result2 !== result3);
    const result4 = windows.next().value;
    sameArray(result4, [4, 5]);
    result4.pop();
    sameArray(result4, [4]);
    assert(result3 !== result4);
    assert(windows.next().done);
}

// invalid

{
    const invalidIterators = [
        1,
        1n,
        true,
        false,
        null,
        undefined,
        Symbol("symbol"),
    ];
    for (const invalidIterator of invalidIterators) {
        shouldThrow(function () {
            Iterator.prototype.windows.call(invalidIterator);
        }, TypeError, "Iterator.prototype.windows requires that |this| be an Object.");
    }
}

{
    const invalidWindowSizes = [
        undefined,
        "test",
        {},
    ];
    const validIter = (function* gen() {})();
    for (const invalidWindowSize of invalidWindowSizes) {
        shouldThrow(function () {
            Iterator.prototype.windows.call(validIter, invalidWindowSize);
        }, RangeError, "Iterator.prototype.windows requires that first argument not be NaN.");
    }
}

{
    const invalidWindowSizes = [
        -1,
        0,
        2 ** 32,
        null,
    ];
    const validIter = (function* gen() {})();
    for (const invalidWindowSize of invalidWindowSizes) {
        shouldThrow(function () {
            Iterator.prototype.windows.call(validIter, invalidWindowSize);
        }, RangeError, "Iterator.prototype.windows requires that first argument be between 1 and 2**32 - 1.");
    }
}

shouldThrow(function () {
    const validIter = (function* gen() {})();
    validIter.windows(1, "invalid");
}, TypeError, "Iterator.prototype.windows requires that second argument be \"only-full\" or \"allow-partial\".");
