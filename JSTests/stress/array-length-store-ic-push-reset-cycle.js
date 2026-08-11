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
let a = [];
for (let i = 0; i < testLoopCount; ++i) {
    for (let j = 0; j < 8; ++j) a.push(j);
    f(a);
    shouldBe(a.length, 0);
    shouldBe(a[0], undefined);
    shouldBe(a[7], undefined);
}
