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
    checkCloses(() => shouldBe(makeIterator("outer", 3).flatMap((x) => makeIterator("inner" + x, x)).toArray().join(), "0,0,1"), "");
    checkCloses(() => {
        var helper = makeIterator("outer", 3).flatMap((x) => makeIterator("inner" + x, 2));
        shouldBe(helper.next().value, 0);
        helper.return();
    }, "inner0,outer");
    checkCloses(() => {
        var helper = makeIterator("outer", 3).flatMap((x) => ({ next() { return { value: 1, done: false }; }, return() { log.push("inner"); throw boom; } }));
        helper.next();
        shouldThrow(() => helper.return(), boom);
    }, "inner,outer");
}
