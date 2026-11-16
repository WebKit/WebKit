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
    let a = [1, 2, 3, 4, 5];
    f(a, i & 1 ? 2 : 7);
    if (i & 1) {
        shouldBe(a.length, 2);
        shouldBe(a[2], undefined);
    } else {
        shouldBe(a.length, 7);
        shouldBe(a[4], 5);
        shouldBe(a[5], undefined);
    }
}
