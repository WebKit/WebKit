function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function shouldThrow(func, expected) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (error !== expected && !(typeof expected === "function" && error instanceof expected))
        throw new Error('bad error: ' + error);
}

var log = [];
function makeIterator(name, n) {
    var i = 0;
    return {
        __proto__: Iterator.prototype,
        next() { return i < n ? { value: i++, done: false } : { done: true }; },
        return() { log.push(name); return {}; }
    };
}

var getterCalls = 0;
function makeIterableWithGetter(n) {
    return {
        get [Symbol.iterator]() {
            getterCalls++;
            return function () { return makeIterator("it", n); };
        }
    };
}

function checkReadsIteratorOnce(func) {
    var before = getterCalls;
    func(makeIterableWithGetter(3));
    shouldBe(getterCalls, before + 1);
}

function checkCloses(func, expectedLog) {
    log = [];
    func();
    shouldBe(log.join(), expectedLog);
}

var boom = new Error("boom");

for (var i = 0; i < testLoopCount; ++i) {
    checkReadsIteratorOnce((items) => shouldBe(Object.keys(Object.groupBy(items, (x) => x & 1)).join(), "0,1"));
    checkReadsIteratorOnce((items) => shouldBe(Map.groupBy(items, (x) => x & 1).size, 2));
    checkReadsIteratorOnce((items) => shouldBe(Int32Array.from(items, (x) => x * 2).join(), "0,2,4"));

    checkCloses(() => shouldThrow(() => Object.groupBy(makeIterableWithGetter(3), (x) => { if (x === 1) throw boom; return x; }), boom), "it");
    checkCloses(() => shouldThrow(() => Object.groupBy(makeIterableWithGetter(3), (x) => ({ toString() { throw boom; } })), boom), "it");
    checkCloses(() => shouldThrow(() => Map.groupBy(makeIterableWithGetter(3), (x) => { if (x === 1) throw boom; return x; }), boom), "it");
}

var nonObjectIterable = { [Symbol.iterator]() { return 1; } };
shouldThrow(() => Object.groupBy(nonObjectIterable, (x) => x), TypeError);
shouldThrow(() => Map.groupBy(nonObjectIterable, (x) => x), TypeError);
shouldThrow(() => Int32Array.from(nonObjectIterable), TypeError);

var nonObjectResult = { [Symbol.iterator]() { return { next() { return 1; } }; } };
shouldThrow(() => Object.groupBy(nonObjectResult, (x) => x), TypeError);
shouldThrow(() => Map.groupBy(nonObjectResult, (x) => x), TypeError);
shouldThrow(() => Int32Array.from(nonObjectResult), TypeError);

var fromAsyncGetterCalls = 0;
var fromAsyncResult = null;
var fromAsyncClosed = 0;
var fromAsyncError = null;
Array.fromAsync({
    get [Symbol.iterator]() {
        fromAsyncGetterCalls++;
        return function () { return makeIterator("it", 3); };
    }
}, (x) => x * 2).then((result) => { fromAsyncResult = result; });
Array.fromAsync({
    [Symbol.iterator]() {
        var i = 0;
        return {
            next() { return { value: i++, done: false }; },
            return() { fromAsyncClosed++; return {}; }
        };
    }
}, (x) => { if (x === 1) throw boom; return x; }).then(() => { }, (error) => { fromAsyncError = error; });
drainMicrotasks();
shouldBe(fromAsyncGetterCalls, 1);
shouldBe(fromAsyncResult.join(), "0,2,4");
shouldBe(fromAsyncClosed, 1);
shouldBe(fromAsyncError, boom);
