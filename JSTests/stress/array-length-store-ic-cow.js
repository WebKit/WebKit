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

function mk() { return [1, 2, 3, 4, 5]; }
noInline(mk);
function f(a, n) { a.length = n; }
noInline(f);
for (let i = 0; i < testLoopCount; ++i) {
    let a = mk();
    let b = mk();
    f(a, 1);
    shouldBe(a.length, 1);
    shouldBe(a[1], undefined);
    shouldBe(b.length, 5);
    shouldBe(b[1], 2);
    shouldBe(b[4], 5);
}
