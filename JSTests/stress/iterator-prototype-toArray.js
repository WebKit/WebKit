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
    const arr = iter.toArray();
    sameArray(arr, [1, 2, 3, 4, 5]);
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
    const arr = Iterator.prototype.toArray.call(iter);
    sameArray(arr, [1, 2, 3, 4, 5]);
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
    const arr = iter.toArray();
    sameValue(nextGetCount, 1);
    sameArray(arr, [1, 2, 3, 4, 5]);
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
    const arr = iter.toArray();
    sameArray(arr, [1, 2, 3, 4, 5]);
}

{
    const arr1 = [1, 2, 3, 4, 5];
    const arr2 = arr1[Symbol.iterator]().toArray();
    sameValue(arr1 === arr2, false);
    sameArray(arr1, arr2);
}

{
    const arr1 = [1, 2, 3, 4, 5];
    shouldThrow(function () {
        Iterator.prototype.toArray.call(arr1);
    }, TypeError, "Type error");
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
            Iterator.prototype.toArray.call(invalidIterator);
        }, TypeError, "Iterator.prototype.toArray requires that |this| be an Object.");
    }
}

{
    const iter = {};
    shouldThrow(function () {
        Iterator.prototype.toArray.call(iter);
    }, TypeError, "Type error");
}

{
    const iter = { next() { return 3; }};
    shouldThrow(function () {
        Iterator.prototype.toArray.call(iter);
    }, TypeError, "Iterator result interface is not an object.")
}
