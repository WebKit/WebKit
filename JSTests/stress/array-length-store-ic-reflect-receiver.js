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

function f(a, n, r) { return Reflect.set(a, "length", n, r); }
noInline(f);
for (let i = 0; i < testLoopCount; ++i) {
    let a = [1, 2, 3];
    let r = {};
    shouldBe(f(a, 1, r), true);
    shouldBe(a.length, 3);
    shouldBe(r.length, 1);
}
