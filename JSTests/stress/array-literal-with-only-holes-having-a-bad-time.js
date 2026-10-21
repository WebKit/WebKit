function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function test(value) {
    let array = [,,,];
    array[0] = value;
    array[1] = value;
    return array;
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i) {
    let array = test(i);
    shouldBe(array.length, 3);
    shouldBe(array[0], i);
    shouldBe(array[1], i);
    shouldBe(2 in array, false);
}

let poked = 0;
Object.defineProperty(Array.prototype, 1, {
    get() { return "getter"; },
    set(value) { poked++; },
    configurable: true,
});

for (let i = 0; i < testLoopCount; ++i) {
    let array = test(i);
    shouldBe(array.length, 3);
    shouldBe(array[0], i);
    shouldBe(array[1], "getter");
    shouldBe(array.hasOwnProperty(1), false);
    shouldBe(2 in array, false);
    shouldBe(poked, i + 1);
}
