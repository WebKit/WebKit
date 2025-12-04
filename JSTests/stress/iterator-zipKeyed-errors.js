//@ requireOptions("--useIteratorZip=1")

function shouldThrow(fn, errorType, message) {
    try {
        fn();
        throw new Error('Expected to throw, but succeeded');
    } catch (e) {
        if (!(e instanceof errorType))
            throw new Error(`Expected ${errorType.name} but got ${e.name}: ${e.message}`);
        if (message !== undefined && e.message !== message)
            throw new Error(`Expected message '${message}' but got '${e.message}'`);
    }
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

{
    shouldThrow(() => Iterator.zipKeyed(null), TypeError, "Iterator.zipKeyed requires iterables to be an object");
    shouldThrow(() => Iterator.zipKeyed(undefined), TypeError, "Iterator.zipKeyed requires iterables to be an object");
    shouldThrow(() => Iterator.zipKeyed(42), TypeError, "Iterator.zipKeyed requires iterables to be an object");
    shouldThrow(() => Iterator.zipKeyed("string"), TypeError, "Iterator.zipKeyed requires iterables to be an object");
    shouldThrow(() => Iterator.zipKeyed(true), TypeError, "Iterator.zipKeyed requires iterables to be an object");
    shouldThrow(() => Iterator.zipKeyed(Symbol()), TypeError, "Iterator.zipKeyed requires iterables to be an object");
}

{
    shouldThrow(() => Iterator.zipKeyed({}, 42), TypeError, "options should be undefined or object");
    shouldThrow(() => Iterator.zipKeyed({}, "string"), TypeError, "options should be undefined or object");
    shouldThrow(() => Iterator.zipKeyed({}, true), TypeError, "options should be undefined or object");
}

{
    shouldThrow(() => Iterator.zipKeyed({}, { mode: 'invalid' }), TypeError, "mode should be 'shortest' or 'longest' or 'strict'");
    shouldThrow(() => Iterator.zipKeyed({}, { mode: 42 }), TypeError, "mode should be 'shortest' or 'longest' or 'strict'");
    shouldThrow(() => Iterator.zipKeyed({}, { mode: null }), TypeError, "mode should be 'shortest' or 'longest' or 'strict'");
}

{
    shouldThrow(() => Iterator.zipKeyed({}, { mode: 'longest', padding: 42 }), TypeError, "padding option should be an object");
    shouldThrow(() => Iterator.zipKeyed({}, { mode: 'longest', padding: "string" }), TypeError, "padding option should be an object");
    shouldThrow(() => Iterator.zipKeyed({}, { mode: 'longest', padding: true }), TypeError, "padding option should be an object");
}

{
    shouldThrow(() => {
        Iterator.zipKeyed({ a: 42 });
    }, TypeError, "GetIteratorFlattenable expects its first argument to be an object");
}

{
    shouldThrow(() => {
        Iterator.zipKeyed({ a: "string" });
    }, TypeError, "GetIteratorFlattenable expects its first argument to be an object");
}

{
    const badIterable = {
        [Symbol.iterator]() { return 42; }
    };
    shouldThrow(() => {
        Iterator.zipKeyed({ a: badIterable });
    }, TypeError, "Iterator is not an object");
}

{
    const throwingNext = {
        [Symbol.iterator]() {
            return {
                next() { throw new Error("next error"); }
            };
        }
    };
    shouldThrow(() => {
        const iter = Iterator.zipKeyed({ a: throwingNext });
        iter.next();
    }, Error, "next error");
}

{
    const badResult = {
        [Symbol.iterator]() {
            return {
                next() { return 42; }
            };
        }
    };
    shouldThrow(() => {
        const iter = Iterator.zipKeyed({ a: badResult });
        iter.next();
    }, TypeError, "Iterator result interface is not an object");
}

{
    let closed = false;
    const iter1 = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { closed = true; return { done: true }; }
    };
    const throwingIterable = {
        [Symbol.iterator]() { throw new Error("iterator error"); }
    };
    shouldThrow(() => {
        Iterator.zipKeyed({ a: iter1, b: throwingIterable });
    }, Error, "iterator error");
    shouldBe(closed, true);
}

{
    shouldThrow(() => {
        const iter = Iterator.zipKeyed({ a: [1, 2], b: [1] }, { mode: 'strict' });
        for (const _ of iter) {}
    }, TypeError, "Iterators in strict mode have different lengths");
}

{
    shouldThrow(() => {
        const iter = Iterator.zipKeyed({ a: [1], b: [1, 2] }, { mode: 'strict' });
        for (const _ of iter) {}
    }, TypeError, "Iterators in strict mode have different lengths");
}

{
    let closed = [];
    const iter1 = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { closed.push('a'); return { done: true }; }
    };
    const iter2 = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 2 }; },
        return() { closed.push('b'); return { done: true }; }
    };
    const throwingIter = {
        [Symbol.iterator]() { return this; },
        next() { throw new Error("next throws"); },
        return() { closed.push('c'); return { done: true }; }
    };
    shouldThrow(() => {
        const iter = Iterator.zipKeyed({ a: iter1, b: iter2, c: throwingIter });
        iter.next();
    }, Error, "next throws");
    shouldBe(closed.length, 2);
    shouldBe(closed[0], 'b');
    shouldBe(closed[1], 'a');
}

{
    let closeCalled = false;
    const throwingClose = {
        [Symbol.iterator]() {
            return {
                next() { return { done: false, value: 1 }; },
                return() { throw new Error("close error"); }
            };
        }
    };
    shouldThrow(() => {
        const iter = Iterator.zipKeyed({ a: throwingClose, b: [1] });
        for (const _ of iter) {}
    }, Error, "close error");
}

{
    let iter1Closed = false;
    let iter2Closed = false;
    const iter1 = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { iter1Closed = true; return { done: true }; }
    };
    const iter2 = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 2 }; },
        return() { iter2Closed = true; return { done: true }; }
    };
    const iter = Iterator.zipKeyed({ a: iter1, b: iter2 });
    iter.next();
    iter.return();
    shouldBe(iter1Closed, true);
    shouldBe(iter2Closed, true);
}

{
    let iter1Closed = false;
    let iter2Closed = false;
    const iter1 = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { iter1Closed = true; return { done: true }; }
    };
    const iter2 = {
        [Symbol.iterator]() { return this; },
        next() { throw new Error("iter2 next error"); },
        return() { iter2Closed = true; return { done: true }; }
    };
    shouldThrow(() => {
        const iter = Iterator.zipKeyed({ a: iter1, b: iter2 });
        iter.next();
    }, Error, "iter2 next error");
    shouldBe(iter1Closed, true);
    shouldBe(iter2Closed, false);
}

{
    let count = 0;
    const iter1 = {
        [Symbol.iterator]() { return this; },
        next() {
            count++;
            if (count <= 2) return { done: false, value: count };
            return { done: true };
        }
    };
    const iter2 = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 'x' }; }
    };
    shouldThrow(() => {
        const iter = Iterator.zipKeyed({ a: iter1, b: iter2 }, { mode: 'strict' });
        for (const _ of iter) {}
    }, TypeError, "Iterators in strict mode have different lengths");
}

{
    let closed = false;
    const iter = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { closed = true; return { done: true }; }
    };
    const zipped = Iterator.zipKeyed({ a: iter, b: [1, 2] });
    try {
        for (const _ of zipped) {
            throw new Error("user error");
        }
    } catch (e) {
        if (e.message !== "user error") throw e;
    }
    shouldBe(closed, true);
}

{
    let closed = false;
    const obj = {};
    Object.defineProperty(obj, 'a', {
        get() { throw new Error("getter error"); },
        enumerable: true
    });
    shouldThrow(() => {
        Iterator.zipKeyed(obj);
    }, Error, "getter error");
}

{
    let closed = false;
    const iter = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { closed = true; return { done: true }; }
    };
    const obj = { a: iter };
    Object.defineProperty(obj, 'b', {
        get() { throw new Error("getter error"); },
        enumerable: true
    });
    shouldThrow(() => {
        Iterator.zipKeyed(obj);
    }, Error, "getter error");
    shouldBe(closed, true);
}

{
    let closed = false;
    const iter = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { closed = true; return { done: true }; }
    };
    const padding = {};
    Object.defineProperty(padding, 'a', {
        get() { throw new Error("padding getter error"); },
        enumerable: true
    });
    shouldThrow(() => {
        Iterator.zipKeyed({ a: iter }, { mode: 'longest', padding });
    }, Error, "padding getter error");
    shouldBe(closed, true);
}

{
    let getOwnPropertyDescriptorCalled = false;
    const proxy = new Proxy({ a: [1, 2] }, {
        getOwnPropertyDescriptor(target, key) {
            getOwnPropertyDescriptorCalled = true;
            throw new Error("getOwnPropertyDescriptor error");
        },
        ownKeys() {
            return ['a'];
        }
    });
    shouldThrow(() => {
        Iterator.zipKeyed(proxy);
    }, Error, "getOwnPropertyDescriptor error");
    shouldBe(getOwnPropertyDescriptorCalled, true);
}

{
    let closed = false;
    const iter = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { closed = true; return { done: true }; }
    };
    const proxy = new Proxy({ a: iter, b: [1, 2] }, {
        getOwnPropertyDescriptor(target, key) {
            if (key === 'b') throw new Error("getOwnPropertyDescriptor error");
            return Object.getOwnPropertyDescriptor(target, key);
        },
        ownKeys() {
            return ['a', 'b'];
        }
    });
    shouldThrow(() => {
        Iterator.zipKeyed(proxy);
    }, Error, "getOwnPropertyDescriptor error");
    shouldBe(closed, true);
}

{
    let closed = false;
    const iter = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { closed = true; return { done: true }; }
    };
    const throwingDone = {
        [Symbol.iterator]() {
            return {
                next() {
                    return {
                        get done() { throw new Error("done getter error"); },
                        value: 1
                    };
                }
            };
        }
    };
    shouldThrow(() => {
        const zipped = Iterator.zipKeyed({ a: iter, b: throwingDone });
        zipped.next();
    }, Error, "done getter error");
    shouldBe(closed, true);
}

{
    let closed = false;
    const iter = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { closed = true; return { done: true }; }
    };
    const throwingValue = {
        [Symbol.iterator]() {
            return {
                next() {
                    return {
                        done: false,
                        get value() { throw new Error("value getter error"); }
                    };
                }
            };
        }
    };
    shouldThrow(() => {
        const zipped = Iterator.zipKeyed({ a: iter, b: throwingValue });
        zipped.next();
    }, Error, "value getter error");
    shouldBe(closed, true);
}
