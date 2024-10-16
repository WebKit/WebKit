//@ requireOptions("--useIteratorHelpers=1", "--useIteratorChunking=1")

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
    const result = Array.from(iter.chunks(1));
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
    const result = Array.from(Iterator.prototype.chunks.call(iter, 2));
    sameArray(result, [[1, 2], [3, 4], [5]]);
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
    const result = Array.from(iter.chunks(3));
    sameValue(nextGetCount, 1);
    sameArray(result, [[1, 2, 3], [4, 5]]);
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
    const result = Array.from(iter.chunks(4));
    sameArray(result, [[1, 2, 3, 4], [5]]);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.chunks === Iterator.prototype.chunks);
    const result = Array.from(iter.chunks(5));
    sameArray(result, [[1, 2, 3, 4, 5]]);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.chunks === Iterator.prototype.chunks);
    const result = Array.from(iter.chunks(6));
    sameArray(result, [[1, 2, 3, 4, 5]]);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.chunks === Iterator.prototype.chunks);
    const chunks = iter.chunks(2);
    const result1 = chunks.next().value;
    sameArray(result1, [1, 2]);
    result1.pop();
    sameArray(result1, [1]);
    const result2 = chunks.next().value;
    sameArray(result2, [3, 4]);
    result2.pop();
    sameArray(result2, [3]);
    assert(result1 !== result2);
    const result3 = chunks.next().value;
    sameArray(result3, [5]);
    result3.pop();
    sameArray(result3, []);
    assert(result2 !== result3);
    assert(chunks.next().done);
}

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
            Iterator.prototype.chunks.call(invalidIterator);
        }, TypeError, "Iterator.prototype.chunks requires that |this| be an Object.");
    }
}

{
    const invalidChunkSizes = [
        undefined,
        "test",
        {},
    ];
    const validIter = (function* gen() {})();
    for (const invalidChunkSize of invalidChunkSizes) {
        shouldThrow(function () {
            Iterator.prototype.chunks.call(validIter, invalidChunkSize);
        }, RangeError, "Iterator.prototype.chunks requires that argument not be NaN.");
    }
}

{
    const invalidChunkSizes = [
        -1,
        0,
        2 ** 32,
        null,
    ];
    const validIter = (function* gen() {})();
    for (const invalidChunkSize of invalidChunkSizes) {
        shouldThrow(function () {
            Iterator.prototype.chunks.call(validIter, invalidChunkSize);
        }, RangeError, "Iterator.prototype.chunks requires that argument be between 1 and 2**32 - 1.");
    }
}
