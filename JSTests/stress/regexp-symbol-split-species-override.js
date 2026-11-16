// Regression test: the optimized RegExp.prototype[Symbol.split] path must honor an
// overwritten Symbol.species on rx.constructor. Here the species constructor returns a
// RegExp that splits on ";", so the result must reflect that splitter and not the
// original "," pattern.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

const rx = /,/;
rx.constructor = {
    [Symbol.species]: function (re, flags) {
        return new RegExp(";", flags);
    }
};

function doSplit(s) {
    return rx[Symbol.split](s);
}
noInline(doSplit);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(JSON.stringify(doSplit("a,b;c,d")), '["a,b","c,d"]');
