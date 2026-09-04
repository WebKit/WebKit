function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function referenceIndexOf(array, value, fromIndex) {
    let length = array.length;
    let index = fromIndex | 0;
    if (index < 0)
        index = Math.max(0, length + index);
    for (; index < length; ++index) {
        if (index in array && array[index] === value)
            return index;
    }
    return -1;
}

function int32IndexOf(array, value) { return array.indexOf(value); }
function int32IndexOfFrom(array, value, fromIndex) { return array.indexOf(value, fromIndex); }
function int32Includes(array, value) { return array.includes(value); }
function int32IncludesFrom(array, value, fromIndex) { return array.includes(value, fromIndex); }
function doubleIndexOf(array, value) { return array.indexOf(value); }
function doubleIndexOfFrom(array, value, fromIndex) { return array.indexOf(value, fromIndex); }
function doubleIncludes(array, value) { return array.includes(value); }
function doubleIncludesFrom(array, value, fromIndex) { return array.includes(value, fromIndex); }
noInline(int32IndexOf);
noInline(int32IndexOfFrom);
noInline(int32Includes);
noInline(int32IncludesFrom);
noInline(doubleIndexOf);
noInline(doubleIndexOfFrom);
noInline(doubleIncludes);
noInline(doubleIncludesFrom);

function makeInt32Array(length) {
    let array = [];
    for (let i = 0; i < length; ++i)
        array.push(i * 3 - 7);
    return array;
}

function makeDoubleArray(length) {
    let array = [];
    for (let i = 0; i < length; ++i)
        array.push(i * 0.5 - 7.25);
    return array;
}

let lengths = [0, 1, 3, 7, 8, 9, 15, 16, 17, 31, 32, 33, 47, 48, 49, 63, 64, 65, 100, 127, 128, 129, 200, 1000];
let holeyInt32 = makeInt32Array(200);
delete holeyInt32[50];
delete holeyInt32[150];
let holeyDouble = makeDoubleArray(200);
delete holeyDouble[50];
delete holeyDouble[150];
holeyDouble[120] = -0;

let cases = [];
for (let length of lengths) {
    let fromIndexes = [0, 1, 5, 40, length - 1, length, length + 5, -1, -3, -40, -length, -length - 1];
    let int32Array = makeInt32Array(length);
    for (let value of [-7, -8, 0, 2, 3 * (length - 1) - 7, 3 * (length >> 1) - 7, 3 * length - 7, 3 * (length - 5) - 7, 2147483647, -2147483648]) {
        cases.push({ array: int32Array, value, expected: referenceIndexOf(int32Array, value, 0) });
        for (let fromIndex of fromIndexes)
            cases.push({ array: int32Array, value, fromIndex, expected: referenceIndexOf(int32Array, value, fromIndex) });
    }
    let doubleArray = makeDoubleArray(length);
    for (let value of [-7.25, -7.5, 0, 0.5 * (length - 1) - 7.25, 0.5 * (length >> 1) - 7.25, 0.5 * length - 7.25, 0.5 * (length - 5) - 7.25, NaN, -0, 1e300]) {
        cases.push({ array: doubleArray, value, expected: referenceIndexOf(doubleArray, value, 0), isDouble: true });
        for (let fromIndex of fromIndexes)
            cases.push({ array: doubleArray, value, fromIndex, expected: referenceIndexOf(doubleArray, value, fromIndex), isDouble: true });
    }
}
for (let value of [143, 443, 0, -7, 590, 1000])
    cases.push({ array: holeyInt32, value, expected: referenceIndexOf(holeyInt32, value, 0) });
for (let value of [17.75, 67.75, NaN, 0, -0, 52.75, 92.75, 100])
    cases.push({ array: holeyDouble, value, expected: referenceIndexOf(holeyDouble, value, 0), isDouble: true });

for (let i = 0; i < testLoopCount / 50; ++i) {
    for (let { array, value, fromIndex, expected, isDouble } of cases) {
        if (fromIndex === undefined) {
            shouldBe((isDouble ? doubleIndexOf : int32IndexOf)(array, value), expected);
            shouldBe((isDouble ? doubleIncludes : int32Includes)(array, value), expected !== -1);
        } else {
            shouldBe((isDouble ? doubleIndexOfFrom : int32IndexOfFrom)(array, value, fromIndex), expected);
            shouldBe((isDouble ? doubleIncludesFrom : int32IncludesFrom)(array, value, fromIndex), expected !== -1);
        }
    }
}
