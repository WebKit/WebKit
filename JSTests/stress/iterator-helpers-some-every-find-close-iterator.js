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
    checkCloses(() => shouldBe(makeIterator("it", 3).some((x) => x === 1), true), "it");
    checkCloses(() => shouldBe(makeIterator("it", 3).some((x) => x === 5), false), "");
    checkCloses(() => shouldBe(makeIterator("it", 3).every((x) => x < 1), false), "it");
    checkCloses(() => shouldBe(makeIterator("it", 3).every((x) => x < 5), true), "");
    checkCloses(() => shouldBe(makeIterator("it", 3).find((x) => x === 2), 2), "it");
    checkCloses(() => shouldBe(makeIterator("it", 3).find((x) => x === 5), undefined), "");
    checkCloses(() => shouldThrow(() => makeIterator("it", 3).some((x) => { throw boom; }), boom), "it");
    checkCloses(() => shouldThrow(() => makeIterator("it", 3).every((x) => { throw boom; }), boom), "it");
    checkCloses(() => shouldThrow(() => makeIterator("it", 3).find((x) => { throw boom; }), boom), "it");
}
