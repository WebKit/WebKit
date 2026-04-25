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
    let a = [1.5, 2.5, 3.5, 4.5];
    f(a, 1);
    shouldBe(a.length, 1);
    shouldBe(a[0], 1.5);
    shouldBe(a[1], undefined);
    shouldBe(a[3], undefined);
}
