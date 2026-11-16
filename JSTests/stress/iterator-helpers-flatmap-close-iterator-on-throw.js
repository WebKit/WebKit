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

function checkCloses(func, expectedLog) {
    log = [];
    func();
    shouldBe(log.join(), expectedLog);
}

var boom = new Error("boom");

for (var i = 0; i < testLoopCount; ++i) {
    checkCloses(() => shouldThrow(() => makeIterator("outer", 3).flatMap((x) => { throw boom; }).next(), boom), "outer");
    checkCloses(() => shouldThrow(() => makeIterator("outer", 3).flatMap((x) => 1).next(), TypeError), "outer");
    checkCloses(() => shouldThrow(() => makeIterator("outer", 3).flatMap((x) => ({ next() { throw boom; }, return() { log.push("inner"); } })).next(), boom), "outer");
    checkCloses(() => shouldThrow(() => makeIterator("outer", 3).flatMap((x) => ({ next() { return { get done() { throw boom; } }; }, return() { log.push("inner"); } })).next(), boom), "outer");
}
