function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ', expected: ' + expected);
}

function forward(array) {
    let result = 0;
    for (let i = 0; i < array.length; ++i)
        result += array[i];
    return result;
}
noInline(forward);

function backward(array) {
    let result = 0;
    for (let i = array.length - 1; i >= 0; --i)
        result += array[i];
    return result;
}
noInline(backward);

function backwardFrom(array, start) {
    let result = 0;
    for (let i = start; i >= 0; --i)
        result += array[i];
    return result;
}
noInline(backwardFrom);

function backwardAhead(array, ahead) {
    let result = 0;
    for (let i = array.length - 4; i >= 0; --i) {
        result += array[i + 3];
        if (ahead)
            result += array[i + 4];
    }
    return result;
}
noInline(backwardAhead);

function forwardBehind(array, behind) {
    let result = 0;
    for (let i = 3; i < array.length; ++i) {
        result += array[i - 3];
        if (behind)
            result += array[i - 4];
    }
    return result;
}
noInline(forwardBehind);

function forwardShrinking(array) {
    let result = 0;
    for (let i = 0; i < array.length; ++i) {
        result += array[i];
        if (i === 2)
            array.length = 4;
    }
    return result;
}
noInline(forwardShrinking);

function mergedUpperBounds(array, flag, i) {
    let length = array.length;
    if (flag) {
        if (i >= length - 10)
            return -1;
    } else {
        if (i >= length - 20)
            return -1;
    }
    if (i < 0)
        return -2;
    return array[i] + array[i + 1] + array[i + 2];
}
noInline(mergedUpperBounds);

function mergedEqualAndUpperBound(array, flag, i) {
    let length = array.length;
    if (flag)
        i = length - 30;
    else if (i >= length - 20)
        return -1;
    if (i < 0)
        return -2;
    return array[i] + array[i + 1] + array[i + 2];
}
noInline(mergedEqualAndUpperBound);

function mergedHugeOffsets(array, flag, i) {
    let length = array.length;
    if (flag) {
        if (i >= length - 0x7ffffff0)
            return -1;
    } else {
        if (i >= length - 0x40000000)
            return -1;
    }
    return array[i + 0x7fffffff];
}
noInline(mergedHugeOffsets);

let array = [];
for (let i = 0; i < 100; ++i)
    array.push(i);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(forward(array), 4950);
    shouldBe(forward([]), 0);
    shouldBe(forward([1, 2, 3]), 6);
    shouldBe(backward(array), 4950);
    shouldBe(backward([]), 0);
    shouldBe(backward([1, 2, 3]), 6);
    shouldBe(backwardFrom(array, 99), 4950);
    shouldBe(backwardFrom(array, -1), 0);
    shouldBe(backwardAhead(array, false), 4947);
    shouldBe(forwardBehind(array, false), 4656);
    shouldBe(forwardShrinking([1, 2, 3, 4, 5, 6, 7, 8]), 10);
    shouldBe(mergedUpperBounds(array, true, i % 90), (i % 90) * 3 + 3);
    shouldBe(mergedUpperBounds(array, false, i % 80), (i % 80) * 3 + 3);
    shouldBe(mergedUpperBounds(array, true, 90), -1);
    shouldBe(mergedUpperBounds(array, false, 80), -1);
    shouldBe(mergedUpperBounds(array, i & 1, -1), -2);
    shouldBe(mergedEqualAndUpperBound(array, true, 0), 213);
    shouldBe(mergedEqualAndUpperBound(array, false, i % 80), (i % 80) * 3 + 3);
    shouldBe(mergedEqualAndUpperBound(array, false, 80), -1);
    shouldBe(mergedHugeOffsets(array, true, -0x7fffffff + (i % 15)), i % 15);
    shouldBe(mergedHugeOffsets(array, false, -0x7fffffff + (i % 100)), i % 100);
    shouldBe(mergedHugeOffsets(array, true, 0), -1);
}

shouldBe(mergedEqualAndUpperBound([1, 2, 3], true, 0), -2);
shouldBe(mergedUpperBounds([1, 2, 3], true, 0), -1);
shouldBe(mergedHugeOffsets(array, false, -0x80000000), undefined);
shouldBe(mergedHugeOffsets(array, false, -0x7fffffff + 100), undefined);

shouldBe(Number.isNaN(backwardFrom(array, 100)), true);
shouldBe(Number.isNaN(backwardFrom(array, 103)), true);
shouldBe(Number.isNaN(backwardAhead(array, true)), true);
shouldBe(Number.isNaN(forwardBehind(array, true)), true);
