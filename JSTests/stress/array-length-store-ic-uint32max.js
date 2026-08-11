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
    let a = [1, 2, 3];
    f(a, 2);
    shouldBe(a.length, 2);
}
let a = [1, 2, 3];
f(a, 4294967295);
shouldBe(a.length, 4294967295);
