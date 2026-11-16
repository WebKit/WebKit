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

function f(a, n) { a.length = n; }
noInline(f);
for (let i = 0; i < testLoopCount; ++i) {
    let a = ["x", {}, "y", null, true];
    f(a, 1);
    shouldBe(a.length, 1);
    shouldBe(a[0], "x");
    shouldBe(a[1], undefined);
}
