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

function f(a) { a.length = 0; }
noInline(f);
for (let i = 0; i < testLoopCount; ++i) {
    let a = [];
    for (let j = 0; j < 10; ++j) a.push(j);
    f(a);
    shouldBe(a.length, 0);
    shouldBe(a[0], undefined);
    shouldBe(a[9], undefined);
}
