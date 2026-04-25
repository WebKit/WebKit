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

let arr = [10, 20, 30, 40, 50];
for (let i = 0; i < testLoopCount; ++i) f(arr, 5);
shouldBe(arr.length, 5);

Object.defineProperty(arr, "length", { writable: false });

f(arr, 1);
shouldBe(arr.length, 5);
shouldBe(arr[0], 10);
shouldBe(arr[4], 50);

for (let i = 0; i < testLoopCount; ++i) {
    f(arr, 1);
    shouldBe(arr.length, 5);
}
