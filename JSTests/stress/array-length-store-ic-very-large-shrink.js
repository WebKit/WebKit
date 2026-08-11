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
    let a = [];
    for (let j = 0; j < 500; ++j) a.push(j);
    f(a, 10);
    shouldBe(a.length, 10);
    shouldBe(a[9], 9);
    shouldBe(a[10], undefined);
    shouldBe(a[499], undefined);
}
