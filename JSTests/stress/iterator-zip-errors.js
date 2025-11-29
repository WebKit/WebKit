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
    shouldThrow(() => Iterator.zip(null), TypeError, "Iterator.zip requires iterables to be an object");
    shouldThrow(() => Iterator.zip(undefined), TypeError, "Iterator.zip requires iterables to be an object");
    shouldThrow(() => Iterator.zip(42), TypeError, "Iterator.zip requires iterables to be an object");
    shouldThrow(() => Iterator.zip("string"), TypeError, "Iterator.zip requires iterables to be an object");
    shouldThrow(() => Iterator.zip(true), TypeError, "Iterator.zip requires iterables to be an object");
    shouldThrow(() => Iterator.zip(Symbol()), TypeError, "Iterator.zip requires iterables to be an object");
}

{
    shouldThrow(() => Iterator.zip([], 42), TypeError, "options should be undefined or object");
    shouldThrow(() => Iterator.zip([], "string"), TypeError, "options should be undefined or object");
    shouldThrow(() => Iterator.zip([], true), TypeError, "options should be undefined or object");
}

{
    shouldThrow(() => Iterator.zip([], { mode: 'invalid' }), TypeError, "mode should be 'shortest' or 'longest' or 'strict'");
    shouldThrow(() => Iterator.zip([], { mode: 42 }), TypeError, "mode should be 'shortest' or 'longest' or 'strict'");
    shouldThrow(() => Iterator.zip([], { mode: null }), TypeError, "mode should be 'shortest' or 'longest' or 'strict'");
}

{
    shouldThrow(() => Iterator.zip([], { mode: 'longest', padding: 42 }), TypeError, "padding option should be an object");
    shouldThrow(() => Iterator.zip([], { mode: 'longest', padding: "string" }), TypeError, "padding option should be an object");
    shouldThrow(() => Iterator.zip([], { mode: 'longest', padding: true }), TypeError, "padding option should be an object");
}

{
    const obj = {};
    obj[Symbol.iterator] = undefined;
    shouldThrow(() => Iterator.zip(obj), TypeError, "Iterator.zip requires that iterables[Symbol.iterator] be a function");
}

{
    const obj = {
        [Symbol.iterator]() { return 42; }
    };
    shouldThrow(() => Iterator.zip(obj), TypeError, "Iterator.zip requires that iterables[Symbol.iterator]() returns an object");
}

{
    shouldThrow(() => {
        Iterator.zip([[1, 2], 42]);
    }, TypeError, "GetIteratorFlattenable expects its first argument to be an object");
}

{
    shouldThrow(() => {
        Iterator.zip([[1, 2], "string"]);
    }, TypeError, "GetIteratorFlattenable expects its first argument to be an object");
}

{
    const badIterable = {
        [Symbol.iterator]() { return 42; }
    };
    shouldThrow(() => {
        Iterator.zip([[1, 2], badIterable]);
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
        const iter = Iterator.zip([throwingNext]);
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
        const iter = Iterator.zip([badResult]);
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
        Iterator.zip([iter1, throwingIterable]);
    }, Error, "iterator error");
    shouldBe(closed, true);
}

{
    shouldThrow(() => {
        const iter = Iterator.zip([[1, 2], [1]], { mode: 'strict' });
        for (const _ of iter) {}
    }, TypeError, "Iterators in strict mode have different lengths");
}

{
    shouldThrow(() => {
        const iter = Iterator.zip([[1], [1, 2]], { mode: 'strict' });
        for (const _ of iter) {}
    }, TypeError, "Iterators in strict mode have different lengths");
}

{
    let closed = [];
    const iter1 = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { closed.push(1); return { done: true }; }
    };
    const iter2 = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 2 }; },
        return() { closed.push(2); return { done: true }; }
    };
    const throwingIter = {
        [Symbol.iterator]() { return this; },
        next() { throw new Error("next throws"); },
        return() { closed.push(3); return { done: true }; }
    };
    shouldThrow(() => {
        const iter = Iterator.zip([iter1, iter2, throwingIter]);
        iter.next();
    }, Error, "next throws");
    shouldBe(closed.length, 2);
    shouldBe(closed[0], 2);
    shouldBe(closed[1], 1);
}

{
    const nonIterablePadding = {};
    nonIterablePadding[Symbol.iterator] = undefined;
    shouldThrow(() => {
        Iterator.zip([[1, 2]], { mode: 'longest', padding: nonIterablePadding });
    }, TypeError, "padding[Symbol.iterator] is not a function");
}

{
    const badPadding = {
        [Symbol.iterator]() { return 42; }
    };
    shouldThrow(() => {
        Iterator.zip([[1, 2]], { mode: 'longest', padding: badPadding });
    }, TypeError, "padding[Symbol.iterator]() did not return an object");
}

{
    const throwingPaddingNext = {
        [Symbol.iterator]() {
            return {
                next() { throw new Error("padding next error"); }
            };
        }
    };
    shouldThrow(() => {
        Iterator.zip([[1, 2]], { mode: 'longest', padding: throwingPaddingNext });
    }, Error, "padding next error");
}

{
    const badPaddingResult = {
        [Symbol.iterator]() {
            return {
                next() { return "not an object"; }
            };
        }
    };
    shouldThrow(() => {
        Iterator.zip([[1, 2]], { mode: 'longest', padding: badPaddingResult });
    }, TypeError, "Iterator result interface is not an object");
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
        const iter = Iterator.zip([throwingClose, [1]]);
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
    const iter = Iterator.zip([iter1, iter2]);
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
        const iter = Iterator.zip([iter1, iter2]);
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
        const iter = Iterator.zip([iter1, iter2], { mode: 'strict' });
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
    const zipped = Iterator.zip([iter, [1, 2]]);
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
    let inputClosed = false;
    let innerClosed = false;
    const inputIter = {
        [Symbol.iterator]() { return this; },
        i: 0,
        next() {
            this.i++;
            if (this.i === 1) return { done: false, value: [1, 2] };
            if (this.i === 2) throw new Error("input iter error");
            return { done: true };
        },
        return() { inputClosed = true; return { done: true }; }
    };
    shouldThrow(() => {
        Iterator.zip(inputIter);
    }, Error, "input iter error");
    shouldBe(inputClosed, false);
    shouldBe(innerClosed, false);
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
        const zipped = Iterator.zip([iter, throwingDone]);
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
        const zipped = Iterator.zip([iter, throwingValue]);
        zipped.next();
    }, Error, "value getter error");
    shouldBe(closed, true);
}

{
    let innerClosed = false;
    const innerIter = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { innerClosed = true; return { done: true }; }
    };
    const inputIterWithThrowingDone = {
        [Symbol.iterator]() { return this; },
        i: 0,
        next() {
            this.i++;
            if (this.i === 1) return { done: false, value: innerIter };
            return {
                get done() { throw new Error("input done getter error"); },
                value: null
            };
        }
    };
    shouldThrow(() => {
        Iterator.zip(inputIterWithThrowingDone);
    }, Error, "input done getter error");
    shouldBe(innerClosed, true);
}

{
    let innerClosed = false;
    const innerIter = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { innerClosed = true; return { done: true }; }
    };
    const inputIterWithThrowingValue = {
        [Symbol.iterator]() { return this; },
        i: 0,
        next() {
            this.i++;
            if (this.i === 1) return { done: false, value: innerIter };
            return {
                done: false,
                get value() { throw new Error("input value getter error"); }
            };
        }
    };
    shouldThrow(() => {
        Iterator.zip(inputIterWithThrowingValue);
    }, Error, "input value getter error");
    shouldBe(innerClosed, true);
}

{
    let closed = false;
    const iter = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { closed = true; return { done: true }; }
    };
    const paddingWithThrowingNext = {
        [Symbol.iterator]() {
            return {
                get next() { throw new Error("padding next getter error"); }
            };
        }
    };
    shouldThrow(() => {
        Iterator.zip([iter], { mode: "longest", padding: paddingWithThrowingNext });
    }, Error, "padding next getter error");
    shouldBe(closed, true);
}
