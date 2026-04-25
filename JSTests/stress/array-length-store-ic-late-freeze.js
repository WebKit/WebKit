"use strict";
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
    f(a, 2);
    shouldBe(a.length, 2);
}

let arr = [1, 2, 3];
Object.defineProperty(arr, "length", { writable: false });
shouldThrow(() => f(arr, 1), TypeError);
shouldBe(arr.length, 3);
shouldBe(arr[0], 1);
shouldBe(arr[2], 3);

for (let i = 0; i < testLoopCount; ++i) {
    shouldThrow(() => f(arr, 1), TypeError);
    shouldBe(arr.length, 3);
}
