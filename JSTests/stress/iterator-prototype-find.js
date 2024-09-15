//@ requireOptions("--useIteratorHelpers=1")

function assert(a, text) {
    if (!a)
        throw new Error(`Failed assertion: ${text}`);
}

function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
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
    const result = iter.find(v => v === 3);
    sameValue(result, 3);
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
    const result = Iterator.prototype.find.call(iter, v => v > 4);
    sameValue(result, 5);
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
    const result = iter.find(v => v === 5);
    sameValue(result, 5);
    sameValue(nextGetCount, 1);
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
    const result = iter.find(v => v > 4);
    sameValue(result, 5);
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
    const result = iter.find(v => v > 5);
    sameValue(result, undefined);
}

{
    const arr = [1, 2, 3, 4, 5];
    const result = arr[Symbol.iterator]().find(v => v === 3);
    sameValue(result, 3);
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
            Iterator.prototype.find.call(invalidIterator, () => true);
        }, TypeError, "Iterator.prototype.find requires that |this| be an Object.");
    }
}

{
    const iter = {};
    shouldThrow(function () {
        Iterator.prototype.find.call(iter, () => true);
    }, TypeError, "undefined is not a function");
}

{
    const iter = { next() { return 3; }};
    shouldThrow(function () {
        Iterator.prototype.find.call(iter, () => true);
    }, TypeError, "Iterator result interface is not an object.");
}

{
    const iter = {
        i: 1,
        next() {
            return { value: this.i++, done: false };
        }
    };
    shouldThrow(function () {
        Iterator.prototype.find.call(iter, 1);
    }, TypeError, "Iterator.prototype.find callback must be a function.");
}

{
    class Iter extends Iterator {
        i = 1;
        next() {
            return { value: this.i++, done: false };
        }
    }
    const iter = new Iter();
    const result = iter.find((v, index) => index === 1 && v === 2);
    sameValue(result, 2);
}
