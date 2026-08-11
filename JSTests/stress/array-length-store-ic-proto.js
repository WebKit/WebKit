function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("got " + actual + ", expected " + expected);
}

function shouldThrow(func, errorType) {
    let threw = null;
    try { func(); } catch (e) { threw = e; }
    if (!threw || threw.constructor !== errorType)
        throw new Error("expected " + errorType.name + ", got " + threw);
}

function f(o, n) { o.length = n; }
noInline(f);
let proto = [1, 2, 3, 4, 5];
for (let i = 0; i < testLoopCount; ++i) f([1, 2, 3], 1);
for (let i = 0; i < testLoopCount; ++i) {
    let o = Object.create(proto);
    f(o, 2);
    shouldBe(o.length, 2);
    shouldBe(proto.length, 5);
}
