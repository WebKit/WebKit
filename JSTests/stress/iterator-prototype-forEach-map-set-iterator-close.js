// Regression test for the Map/Set iterator fast path in forEachInIteratorProtocol.
// When the callback passed to Iterator.prototype.forEach throws, the iterator's
// "return" method (IteratorClose) must be invoked, even for Map/Set iterators
// that have an own "return" property.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function shouldThrow(func, errorMessage) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown");
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}, expected: ${errorMessage}`);
}

// Map values iterator with own "return": callback throws -> return must be called.
{
    const map = new Map([[1, "a"], [2, "b"], [3, "c"]]);
    const iterator = map.values();
    let returnCalled = 0;
    let returnThisValue = null;
    iterator.return = function () {
        ++returnCalled;
        returnThisValue = this;
        return { done: true, value: undefined };
    };
    shouldThrow(() => {
        iterator.forEach(() => { throw new Error("callback error"); });
    }, "Error: callback error");
    shouldBe(returnCalled, 1);
    shouldBe(returnThisValue, iterator);
}

// Map keys iterator with own "return".
{
    const map = new Map([[1, "a"], [2, "b"]]);
    const iterator = map.keys();
    let returnCalled = 0;
    iterator.return = function () {
        ++returnCalled;
        return { done: true, value: undefined };
    };
    shouldThrow(() => {
        iterator.forEach(() => { throw new Error("callback error"); });
    }, "Error: callback error");
    shouldBe(returnCalled, 1);
}

// Map entries iterator with own "return".
{
    const map = new Map([[1, "a"], [2, "b"]]);
    const iterator = map.entries();
    let returnCalled = 0;
    iterator.return = function () {
        ++returnCalled;
        return { done: true, value: undefined };
    };
    shouldThrow(() => {
        iterator.forEach(() => { throw new Error("callback error"); });
    }, "Error: callback error");
    shouldBe(returnCalled, 1);
}

// Set values iterator with own "return": callback throws -> return must be called.
{
    const set = new Set([1, 2, 3]);
    const iterator = set.values();
    let returnCalled = 0;
    let returnThisValue = null;
    iterator.return = function () {
        ++returnCalled;
        returnThisValue = this;
        return { done: true, value: undefined };
    };
    shouldThrow(() => {
        iterator.forEach(() => { throw new Error("callback error"); });
    }, "Error: callback error");
    shouldBe(returnCalled, 1);
    shouldBe(returnThisValue, iterator);
}

// Set keys iterator with own "return".
{
    const set = new Set([1, 2]);
    const iterator = set.keys();
    let returnCalled = 0;
    iterator.return = function () {
        ++returnCalled;
        return { done: true, value: undefined };
    };
    shouldThrow(() => {
        iterator.forEach(() => { throw new Error("callback error"); });
    }, "Error: callback error");
    shouldBe(returnCalled, 1);
}

// Set entries iterator with own "return".
{
    const set = new Set([1, 2]);
    const iterator = set.entries();
    let returnCalled = 0;
    iterator.return = function () {
        ++returnCalled;
        return { done: true, value: undefined };
    };
    shouldThrow(() => {
        iterator.forEach(() => { throw new Error("callback error"); });
    }, "Error: callback error");
    shouldBe(returnCalled, 1);
}

// The callback throws after consuming some elements: return is called once,
// and iteration stops at the throwing element.
{
    const map = new Map([[1, "a"], [2, "b"], [3, "c"]]);
    const iterator = map.keys();
    let returnCalled = 0;
    iterator.return = function () {
        ++returnCalled;
        return { done: true, value: undefined };
    };
    const seen = [];
    shouldThrow(() => {
        iterator.forEach((key) => {
            seen.push(key);
            if (key === 2)
                throw new Error("stop");
        });
    }, "Error: stop");
    shouldBe(seen.join(","), "1,2");
    shouldBe(returnCalled, 1);
}

// When both the callback and "return" throw, the callback's exception wins.
{
    const set = new Set([1, 2, 3]);
    const iterator = set.values();
    let returnCalled = 0;
    iterator.return = function () {
        ++returnCalled;
        throw new Error("return error");
    };
    shouldThrow(() => {
        iterator.forEach(() => { throw new Error("callback error"); });
    }, "Error: callback error");
    shouldBe(returnCalled, 1);
}

// "return" is not called when iteration completes normally.
{
    const map = new Map([[1, "a"], [2, "b"]]);
    const iterator = map.values();
    let returnCalled = 0;
    iterator.return = function () {
        ++returnCalled;
        return { done: true, value: undefined };
    };
    const seen = [];
    iterator.forEach((value) => { seen.push(value); });
    shouldBe(seen.join(","), "a,b");
    shouldBe(returnCalled, 0);
}

// Non-JS (bound) callback to cover the non-CachedCall path.
{
    const map = new Map([[1, "a"], [2, "b"]]);
    const iterator = map.values();
    let returnCalled = 0;
    iterator.return = function () {
        ++returnCalled;
        return { done: true, value: undefined };
    };
    const callback = function () { throw new Error(this.message); }.bind({ message: "bound callback error" });
    shouldThrow(() => {
        iterator.forEach(callback);
    }, "Error: bound callback error");
    shouldBe(returnCalled, 1);
}

// Fast path without own "return" still works (no regression).
{
    const map = new Map([[1, "a"], [2, "b"], [3, "c"]]);
    const seen = [];
    map.values().forEach((value) => { seen.push(value); });
    shouldBe(seen.join(","), "a,b,c");

    const set = new Set([1, 2, 3]);
    let sum = 0;
    set.values().forEach((value) => { sum += value; });
    shouldBe(sum, 6);
}

// Fast path without own "return": callback exceptions still propagate.
{
    const set = new Set([1, 2, 3]);
    const seen = [];
    shouldThrow(() => {
        set.values().forEach((value) => {
            seen.push(value);
            if (value === 2)
                throw new Error("no return");
        });
    }, "Error: no return");
    shouldBe(seen.join(","), "1,2");
}

// Repeat to make sure JIT tiers behave the same.
for (let i = 0; i < 1e3; ++i) {
    const map = new Map([[i, i * 10], [i + 1, (i + 1) * 10]]);
    const iterator = map.values();
    let returnCalled = 0;
    iterator.return = function () {
        ++returnCalled;
        return { done: true, value: undefined };
    };
    shouldThrow(() => {
        iterator.forEach(() => { throw new Error("loop error"); });
    }, "Error: loop error");
    shouldBe(returnCalled, 1);
}
