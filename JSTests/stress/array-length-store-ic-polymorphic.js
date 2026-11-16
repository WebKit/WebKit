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

function f(a, n) { a.length = n; return a.length; }
noInline(f);
for (let i = 0; i < testLoopCount; ++i) f([1, 2, 3, 4, 5], 2);
for (let i = 0; i < testLoopCount; ++i) {
    let which = i & 3;
    if (which === 0) shouldBe(f([1, 2, 3, 4, 5], 2), 2);
    else if (which === 1) shouldBe(f([1.1, 2.2, 3.3], 1), 1);
    else if (which === 2) shouldBe(f({ length: 9 }, 3), 3);
    else { let a = [1, 2, 3]; a[100] = 7; shouldBe(f(a, 2), 2); }
}
