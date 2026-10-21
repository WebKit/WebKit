function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function one() { return [,]; }
noInline(one);

function two() { return [,,]; }
noInline(two);

function seven() { return [,,,,,,,]; }
noInline(seven);

function fillInt32(value) {
    let array = [,,,,];
    array[1] = value;
    return array;
}
noInline(fillInt32);

function fillDouble(value) {
    let array = [,,,,];
    array[1] = value + 0.5;
    return array;
}
noInline(fillDouble);

function fillContiguous(value) {
    let array = [,,,,];
    array[1] = String(value);
    return array;
}
noInline(fillContiguous);

function fillMixed(value) {
    let array = [,,,,];
    array[1] = value;
    return array;
}
noInline(fillMixed);

function ignored() {
    [,,];
    return [,,,].length;
}
noInline(ignored);

function grow(value) {
    let array = [,,];
    array.push(value);
    array[5] = value;
    return array;
}
noInline(grow);

function leading(value) { return [,,value]; }
noInline(leading);

function trailing(value) { return [value,,,]; }
noInline(trailing);

function checkHoles(array, length, filled) {
    shouldBe(Array.isArray(array), true);
    shouldBe(Object.getPrototypeOf(array), Array.prototype);
    shouldBe(array.length, length);
    for (let i = 0; i < length; ++i) {
        if (filled.includes(i))
            continue;
        shouldBe(i in array, false);
        shouldBe(array[i], undefined);
    }
    shouldBe(Object.keys(array).length, filled.length);
}

const mixed = [1, 1.5, "string", { }, undefined, null, 2n];

for (let i = 0; i < testLoopCount; ++i) {
    checkHoles(one(), 1, []);
    checkHoles(two(), 2, []);
    checkHoles(seven(), 7, []);

    let int32 = fillInt32(i);
    shouldBe(int32[1], i);
    checkHoles(int32, 4, [1]);

    let double = fillDouble(i);
    shouldBe(double[1], i + 0.5);
    checkHoles(double, 4, [1]);

    let contiguous = fillContiguous(i);
    shouldBe(contiguous[1], String(i));
    checkHoles(contiguous, 4, [1]);

    let value = mixed[i % mixed.length];
    let array = fillMixed(value);
    shouldBe(array[1], value);
    checkHoles(array, 4, [1]);

    shouldBe(ignored(), 3);

    let grown = grow(i);
    shouldBe(grown[2], i);
    shouldBe(grown[5], i);
    checkHoles(grown, 6, [2, 5]);

    let lead = leading(i);
    shouldBe(lead[2], i);
    checkHoles(lead, 3, [2]);

    let trail = trailing(i);
    shouldBe(trail[0], i);
    checkHoles(trail, 3, [0]);

    shouldBe(two() === two(), false);
}

for (let length of [1, 8, 1000, 99999, 100000, 100001, 200000]) {
    let array = (0, eval)("[" + ",".repeat(length) + "]");
    shouldBe(array.length, length);
    shouldBe(0 in array, false);
    shouldBe(length - 1 in array, false);
    shouldBe(Object.keys(array).length, 0);
    array[length - 1] = length;
    shouldBe(array[length - 1], length);
    shouldBe(array.length, length);
}

Array.prototype[1] = "prototype";
for (let i = 0; i < testLoopCount; ++i) {
    let array = two();
    shouldBe(array[0], undefined);
    shouldBe(array[1], "prototype");
    shouldBe(array.hasOwnProperty(1), false);
}
delete Array.prototype[1];

let originalArray = Array;
globalThis.Array = function () { throw new Error("should not be called"); };
for (let i = 0; i < testLoopCount; ++i) {
    let array = two();
    shouldBe(array.length, 2);
    shouldBe(array.constructor, originalArray);
}
globalThis.Array = originalArray;
