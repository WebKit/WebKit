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
    let nextGetCount = 0;
    class Iter extends Iterator {
        i = 1;
        get next() {
            nextGetCount++;
            return function() {
                if (this.i > 5)
                    return { value: this.i, done: true }
                return { value: this.i++, done: false }
            };
        };
    }
    const iter = new Iter();
    const counters = [];
    const sum = iter.reduce((acc, value, i) => {
        counters.push(i);
        return acc + value
    }, 0);
    sameValue(nextGetCount, 1);
    sameValue(sum, 15);
    sameArray(counters, [0, 1, 2, 3, 4]);
}

{
    class Iter extends Iterator {
        i = 1;
        next() {
            if (this.i > 1)
                return { value: this.i, done: true }
            return { value: this.i++, done: false }
        }
    }
    const iter = new Iter();
    let reducerCalls = 0;
    const initialValue = {};
    iter.reduce((acc, value, i) => {
        sameValue(acc, initialValue);
        reducerCalls++;
    }, initialValue);
    sameValue(reducerCalls, 1);
}

{
    class Iter extends Iterator {
        i = 1;
        next() {
            if (this.i > 5)
                return { value: this.i, done: true }
            return { value: this.i++, done: false }
        }
    };
    const iter = new Iter();
    const counters = [];
    const sum = iter.reduce((acc, value, i) => {
        counters.push(i);
        return acc + value
    });
    sameValue(sum, 15);
    sameArray(counters, [1, 2, 3, 4]);
}

{
    class Iter extends Iterator {
        next() { return { value: 1, done: true }; }
    };
    const iter = new Iter();
    shouldThrow(() => {
        iter.reduce(() => {});
    }, TypeError, "Iterator.prototype.reduce requires an initial value or an iterator that is not done.");
}

{
    class Iter extends Iterator {
        next() { return 3; }
    };
    const iter = new Iter();
    shouldThrow(() => {
        iter.reduce(() => {});
    }, TypeError, "Iterator result interface is not an object.");
}

{
    let nextGetCount = 0;
    let returnGetCount = 0;
    class Iter extends Iterator {
        get next() {
            nextGetCount++;
            return function () {
                return { value: 2, done: false }
            }
        };
        get return() {
            returnGetCount++;
            return function() {
                return {};
            };
        };
    };
    const iter = new Iter();
    shouldThrow(() => {
        iter.reduce(() => {
            sameValue(returnGetCount, 0);
            sameValue(nextGetCount, 1);
            throw new Error("my error");
        });
    }, Error, "my error");
    sameValue(nextGetCount, 1);
    sameValue(returnGetCount, 1);
}

{
    let finallyReached = false;
    let yield3Reached = false;
    const gen = (function*() {
        yield 1;

        try { yield 2; }
        finally { finallyReached = true; }

        yield3Reached = true;
        yield 3;
    })();

    shouldThrow(() => {
        gen.reduce((_, value) => {
            if (value === 2)
                throw new Error("my error");
        });
    }, Error, "my error");

    assert(finallyReached, "Generator.prototype.return() invoked the generator function");
    sameValue(yield3Reached, false);
}
