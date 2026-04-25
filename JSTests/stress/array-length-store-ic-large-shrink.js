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
    for (let j = 0; j < 100; ++j) a.push(j);
    f(a, 50);
    shouldBe(a.length, 50);
    shouldBe(a[49], 49);
    shouldBe(a[50], undefined);
    shouldBe(a[99], undefined);
    a.push(777);
    shouldBe(a[50], 777);
    shouldBe(a.length, 51);
}
