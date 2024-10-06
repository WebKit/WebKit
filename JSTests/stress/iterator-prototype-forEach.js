//@ requireOptions("--useIteratorHelpers=1")

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
    const result = [];
    iter.forEach((item, i) => { result.push([item, i]); });
    sameArray(result, [[1, 0], [2, 1], [3, 2], [4, 3], [5, 4]]);
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
    const result = [];
    Iterator.prototype.forEach.call(iter, (item, i) => { result.push([item, i]); });
    sameArray(result, [[1, 0], [2, 1], [3, 2], [4, 3], [5, 4]]);
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
    const result = [];
    sameValue(nextGetCount, 0);
    iter.forEach((item, i) => { result.push([item, i]); });
    sameValue(nextGetCount, 1);
    sameArray(result, [[1, 0], [2, 1], [3, 2], [4, 3], [5, 4]]);
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
    const result = [];
    iter.forEach((item, i) => { result.push([item, i]); });
    sameArray(result, [[1, 0], [2, 1], [3, 2], [4, 3], [5, 4]]);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.forEach === Iterator.prototype.forEach);
    const result = [];
    iter.forEach((item, i) => { result.push([item, i]); });
    sameArray(result, [[1, 0], [2, 1], [3, 2], [4, 3], [5, 4]]);
}

{
    const arr = [1, 2, 3, 4, 5];
    const iter = arr[Symbol.iterator]();
    assert(iter.forEach === Iterator.prototype.forEach);
    const result = [];
    iter.forEach(new Proxy((item, i) => { result.push([item, i]); }, {}));
    sameArray(result, [[1, 0], [2, 1], [3, 2], [4, 3], [5, 4]]);
}

{
    const arr = [1, 2, 3, 4, 5];
    shouldThrow(function () {
        Iterator.prototype.forEach.call(arr, (item, i) => {});
    }, TypeError, "Type error")
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
            Iterator.prototype.forEach.call(invalidIterator);
        }, TypeError, "Iterator.prototype.forEach requires that |this| be an Object.");
    }
}

{
    const invalidCallbacks = [
        1,
        1n,
        true,
        false,
        null,
        undefined,
        Symbol("symbol"),
    ];
    const validIter = (function* gen() {})();
    for (const invalidCallback of invalidCallbacks) {
        shouldThrow(function () {
            Iterator.prototype.forEach.call(validIter, invalidCallback);
        }, TypeError, "Iterator.prototype.forEach requires the callback argument to be callable.");
    }
}

{
    const iter = {};
    shouldThrow(function () {
        Iterator.prototype.forEach.call(iter, () => {});
    }, TypeError, "Type error");
}

{
    const iter = { next() { return 3; }};
    shouldThrow(function () {
        Iterator.prototype.forEach.call(iter, () => {});
    }, TypeError, "Iterator result interface is not an object.")
}

{
    class MyError extends Error {}
    shouldThrow(function() {
        const validIter = (function* gen() { yield 1; })();
        validIter.forEach((item, i) => {
            sameValue(item, 1);
            sameValue(i, 0);
            throw new MyError("my error");
        });
    }, MyError, "my error");
}
