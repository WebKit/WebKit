//@ requireOptions("--useIteratorJoin=1")

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

sameValue([].values().join(), "");
sameValue([].values().join(""), "");
sameValue([].values().join(", "), "");

{
    let called = false;
    sameValue([].values().join({ toString() { called = true; return ", "; } }), "");
    sameValue(called, true);
}

sameValue([1].values().join(), "1");
sameValue(["a"].values().join(), "a");
sameValue([true].values().join(), "true");
sameValue([false].values().join(), "false");
sameValue([null].values().join(), "");
sameValue([undefined].values().join(), "");

sameValue([{ toString() { return 1; } }].values().join(), "1");
sameValue([{ toString() { return true; } }].values().join(), "true");
sameValue([{ toString() { return false; } }].values().join(), "false");
sameValue([{ toString() { return null; } }].values().join(), "null");
sameValue([{ toString() { return undefined; } }].values().join(), "undefined");
sameValue([{ toString() { return ""; } }].values().join(), "");
sameValue([{ toString() { return "a"; } }].values().join(), "a");
sameValue([{ toString() { return "abc"; } }].values().join(), "abc");

sameValue([1, undefined, 2, null, 3].values().join(), "1,2,3");
sameValue([1, undefined, 2, null, 3].values().join(0), "10203");
sameValue([1, undefined, 2, null, 3].values().join(null), "1null2null3");
sameValue([1, undefined, 2, null, 3].values().join(undefined), "1,2,3");
sameValue([1, undefined, 2, null, 3].values().join(""), "123");
sameValue([1, undefined, 2, null, 3].values().join(","), "1,2,3");
sameValue([1, undefined, 2, null, 3].values().join(", "), "1, 2, 3");

sameValue([1, undefined, 2, null, 3].values().join({ toString() { return 0; } }), "10203");
sameValue([1, undefined, 2, null, 3].values().join({ toString() { return true; } }), "1true2true3");
sameValue([1, undefined, 2, null, 3].values().join({ toString() { return false; } }), "1false2false3");
sameValue([1, undefined, 2, null, 3].values().join({ toString() { return null; } }), "1null2null3");
sameValue([1, undefined, 2, null, 3].values().join({ toString() { return undefined; } }), "1undefined2undefined3");
sameValue([1, undefined, 2, null, 3].values().join({ toString() { return ""; } }), "123");
sameValue([1, undefined, 2, null, 3].values().join({ toString() { return ","; } }), "1,2,3");
sameValue([1, undefined, 2, null, 3].values().join({ toString() { return ", "; } }), "1, 2, 3");

{
    let nextGetCount = 0;
    class TestIterator extends Iterator {
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
    const iterator = new TestIterator;
    sameValue(nextGetCount, 0);
    sameValue(iterator.join(", "), "1, 2, 3, 4, 5");
    sameValue(nextGetCount, 1);
}

{
    function* gen() {
        yield undefined;
        yield null;
        yield 1;
        yield 2;
        yield 3;
        yield 4;
        yield 5;
        yield null;
        yield undefined;
    }
    sameValue(gen().join("|"), "1|2|3|4|5");
}

{
    const arr = [undefined, null, 1, 2, 3, 4, 5, null, undefined];
    const iterator = arr[Symbol.iterator]();
    assert(iterator.join === Iterator.prototype.join);
    sameValue(iterator.join(""), "12345");
}

{
    class TestIterator extends Iterator {
        value = 0;
        isClosed = false;
        isDone = false;
        next() {
            return {
                done: false,
                value: ++this.value,
            };
        }
        return() {
            this.isClosed = true;
            return {
                done: true,
                value: undefined,
            };
        }
    }
    const iterator = new TestIterator;
    try {
        iterator.join(Symbol(42));
    } catch (e) {
        sameValue(e.name, "TypeError");
        sameValue(e.message, "Cannot convert a symbol to a string");
    }
    sameValue(iterator.isDone, false);
    sameValue(iterator.isClosed, true);
}

{
    class TestIterator extends Iterator {
        value = 0;
        isClosed = false;
        isDone = false;
        next() {
            return {
                done: false,
                value: Symbol(++this.value),
            };
        }
        return() {
            this.isClosed = true;
            return {
                done: true,
                value: undefined,
            };
        }
    }
    const iterator = new TestIterator;
    try {
        iterator.join();
    } catch (e) {
        sameValue(e.name, "TypeError");
        sameValue(e.message, "Cannot convert a symbol to a string");
    }
    sameValue(iterator.isDone, false);
    sameValue(iterator.isClosed, true);
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
            Iterator.prototype.join.call(invalidIterator);
        }, TypeError, "Iterator.prototype.join requires that |this| be an Object.");
    }
}
