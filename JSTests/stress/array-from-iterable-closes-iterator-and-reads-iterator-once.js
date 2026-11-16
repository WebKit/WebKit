function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

var closed = 0;
function makeIterable(n) {
    return {
        [Symbol.iterator]() {
            var i = 0;
            return {
                next() { return i < n ? { value: i++, done: false } : { done: true }; },
                return() { closed++; return {}; }
            };
        }
    };
}

var iteratorGetterCalls = 0;
var iterableWithGetter = {
    get [Symbol.iterator]() {
        iteratorGetterCalls++;
        return makeIterable(2)[Symbol.iterator];
    }
};

var set = new Set(["a", "b"]);
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(Array.from(makeIterable(3), (x, k) => x * 2 + k).join(), "0,3,6");
    shouldBe(Array.from(set, (x) => x + x).join(), "aa,bb");

    var before = closed;
    var thrown = null;
    try {
        Array.from(makeIterable(3), (x) => { if (x === 1) throw new Error("boom"); return x; });
    } catch (error) {
        thrown = error;
    }
    shouldBe(thrown.message, "boom");
    shouldBe(closed, before + 1);

    var getterCallsBefore = iteratorGetterCalls;
    shouldBe(Array.from(iterableWithGetter).length, 2);
    shouldBe(iteratorGetterCalls, getterCallsBefore + 1);
}

var nonObjectIterator = { [Symbol.iterator]() { return 1; } };
var error = null;
try {
    Array.from(nonObjectIterator);
} catch (e) {
    error = e;
}
shouldBe(error instanceof TypeError, true);

var nonObjectResult = { [Symbol.iterator]() { return { next() { return 1; } }; } };
error = null;
try {
    Array.from(nonObjectResult);
} catch (e) {
    error = e;
}
shouldBe(error instanceof TypeError, true);
