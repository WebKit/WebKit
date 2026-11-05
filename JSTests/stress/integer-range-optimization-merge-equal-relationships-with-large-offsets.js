function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ', expected: ' + expected);
}

function mergedBelow(array, flag, extra) {
    let length = array.length;
    let i = flag ? length - 30 : length - 20;
    if (i < 0)
        return -1;
    return array[i] + array[i + 1] + array[i + extra];
}
noInline(mergedBelow);

function mergedJustBelow(array, flag) {
    let length = array.length;
    let i = flag ? length - 3 : length - 4;
    if (i < 0)
        return -1;
    return array[i] + array[i + 1] + array[i + 2] + array[i + 3];
}
noInline(mergedJustBelow);

function mergedAbove(array, flag, back) {
    let length = array.length;
    let i = flag ? length + 2 : length + 3;
    return array[i - back];
}
noInline(mergedAbove);

function mergedAroundZero(array, flag) {
    let length = array.length;
    let i = flag ? length - 3 : length + 3;
    if (i < 0)
        return -1;
    return array[i];
}
noInline(mergedAroundZero);

function mergedLength(array, i, flag) {
    let length = flag ? i + 3 : i + 4;
    if (length !== array.length)
        return -1;
    if (i < 0)
        return -2;
    return array[i] + array[i + 1] + array[i + 2] + array[i + 3];
}
noInline(mergedLength);

function mergedHugeOffsets(array, flag) {
    let length = array.length;
    let i = flag ? length - 0x7fffffff : length - 0x7ffffffe;
    return array[i + 0x7ffffffe];
}
noInline(mergedHugeOffsets);

function mergedMinOffset(array, flag) {
    let length = array.length;
    let i = flag ? length + -0x80000000 : length + -0x7fffffff;
    return array[i + 0x7fffffff];
}
noInline(mergedMinOffset);

function backwardFrom3(array) {
    let result = 0;
    for (let i = array.length - 3; i >= 0; --i)
        result += array[i];
    return result;
}
noInline(backwardFrom3);

function backwardFrom5Step2(array) {
    let result = 0;
    for (let i = array.length - 5; i >= 0; i -= 2)
        result += array[i];
    return result;
}
noInline(backwardFrom5Step2);

function backwardWindow(array, ahead) {
    let result = 0;
    for (let i = array.length - 3; i >= 0; --i) {
        result += array[i] + array[i + 1] + array[i + 2];
        if (ahead)
            result += array[i + 3];
    }
    return result;
}
noInline(backwardWindow);

function backwardShrinking(array) {
    let result = 0;
    for (let i = array.length - 3; i >= 0; --i) {
        if (i === 4)
            array.length = 2;
        let value = array[i];
        if (value !== undefined)
            result += value;
    }
    return result;
}
noInline(backwardShrinking);

function backwardString(string) {
    let result = 0;
    for (let i = string.length - 3; i >= 0; --i)
        result += string.charCodeAt(i);
    return result;
}
noInline(backwardString);

function forwardFrom3(array, start) {
    let result = 0;
    for (let i = start + 3; i < array.length; ++i)
        result += array[i];
    return result;
}
noInline(forwardFrom3);

let array = [];
for (let i = 0; i < 100; ++i)
    array.push(i);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(mergedBelow(array, true, 2), 213);
    shouldBe(mergedBelow(array, false, 2), 243);
    shouldBe(mergedBelow(array, false, 19), 260);
    shouldBe(mergedJustBelow([1, 2, 3, 4, 5], false), 14);
    shouldBe(mergedAbove(array, true, 3), 99);
    shouldBe(mergedAbove(array, false, 4), 99);
    shouldBe(mergedAroundZero(array, true), 97);
    shouldBe(mergedLength(array, 96, false), 390);
    shouldBe(mergedLength(array, 96, true), -1);
    shouldBe(mergedLength(array, -1, i & 1), -1);
    shouldBe(mergedHugeOffsets(array, true), 99);
    shouldBe(mergedMinOffset(array, true), 99);
    shouldBe(backwardFrom3(array), 4753);
    shouldBe(backwardFrom3([1, 2]), 0);
    shouldBe(backwardFrom3([1, 2, 3]), 1);
    shouldBe(backwardFrom5Step2(array), 2304);
    shouldBe(backwardFrom5Step2([1, 2, 3, 4]), 0);
    shouldBe(backwardWindow(array, false), 14553);
    shouldBe(backwardShrinking([1, 2, 3, 4, 5, 6, 7, 8]), 9);
    shouldBe(backwardString("abcdef"), 394);
    shouldBe(backwardString("ab"), 0);
    shouldBe(forwardFrom3(array, 0), 4947);
    shouldBe(forwardFrom3(array, 97), 0);
}

shouldBe(mergedBelow([1, 2, 3], true, 2), -1);
shouldBe(mergedBelow([1, 2, 3], false, 2), -1);
shouldBe(Number.isNaN(mergedBelow(array, false, 20)), true);
shouldBe(Number.isNaN(mergedBelow(array, true, 30)), true);
shouldBe(Number.isNaN(mergedJustBelow([1, 2, 3, 4, 5], true)), true);
shouldBe(mergedJustBelow([1, 2], true), -1);
shouldBe(mergedAbove(array, true, 2), undefined);
shouldBe(mergedAbove(array, false, 3), undefined);
shouldBe(mergedAbove(array, false, 0), undefined);
shouldBe(mergedAroundZero(array, false), undefined);
shouldBe(mergedAroundZero([1, 2], true), -1);
shouldBe(Number.isNaN(mergedLength(array, 97, true)), true);
shouldBe(mergedLength([1, 2, 3], -1, false), -2);
shouldBe(mergedHugeOffsets(array, false), undefined);
shouldBe(mergedMinOffset(array, false), undefined);
shouldBe(mergedMinOffset([], true), undefined);
shouldBe(Number.isNaN(backwardWindow(array, true)), true);
shouldBe(Number.isNaN(forwardFrom3(array, -4)), true);
