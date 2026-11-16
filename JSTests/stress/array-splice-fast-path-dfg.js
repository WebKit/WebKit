function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i) {
        shouldBe(i in actual, i in expected);
        shouldBe(Object.is(actual[i], expected[i]), true);
    }
}

function makeArray(values) {
    var result = values.slice();
    if (result.length)
        result[0] = values[0];
    return result;
}
noDFG(makeArray);
noFTL(makeArray);

function referenceSplice(values, start, deleteCount, items) {
    var array = makeArray(values);
    var length = array.length;
    start = start < 0 ? Math.max(length + start, 0) : Math.min(start, length);
    deleteCount = Math.min(Math.max(deleteCount, 0), length - start);
    var removed = [];
    removed.length = deleteCount;
    for (var i = 0; i < deleteCount; ++i) {
        if ((start + i) in array)
            removed[i] = array[start + i];
    }
    var result = [];
    for (var i = 0; i < start; ++i) {
        if (i in array)
            result[i] = array[i];
    }
    for (var i = 0; i < items.length; ++i)
        result[start + i] = items[i];
    for (var i = start + deleteCount; i < length; ++i) {
        if (i in array)
            result[i - deleteCount + items.length] = array[i];
    }
    result.length = length - deleteCount + items.length;
    return { array: result, removed };
}
noDFG(referenceSplice);
noFTL(referenceSplice);

function splice0(array, start, deleteCount) {
    array.splice(start, deleteCount);
}
noInline(splice0);

function splice1(array, start, deleteCount, a) {
    array.splice(start, deleteCount, a);
}
noInline(splice1);

function splice2(array, start, deleteCount, a, b) {
    array.splice(start, deleteCount, a, b);
}
noInline(splice2);

function splice3(array, start, deleteCount, a, b, c) {
    array.splice(start, deleteCount, a, b, c);
}
noInline(splice3);

function splice0Result(array, start, deleteCount) {
    return array.splice(start, deleteCount);
}
noInline(splice0Result);

function splice2Result(array, start, deleteCount, a, b) {
    return array.splice(start, deleteCount, a, b);
}
noInline(splice2Result);

function runCase({ values, start, deleteCount, items, expected }) {
    var array = makeArray(values);
    switch (items.length) {
    case 0:
        splice0(array, start, deleteCount);
        break;
    case 1:
        splice1(array, start, deleteCount, items[0]);
        break;
    case 2:
        splice2(array, start, deleteCount, items[0], items[1]);
        break;
    case 3:
        splice3(array, start, deleteCount, items[0], items[1], items[2]);
        break;
    }
    shouldBeArray(array, expected.array);

    array = makeArray(values);
    var removed;
    switch (items.length) {
    case 0:
        removed = splice0Result(array, start, deleteCount);
        break;
    case 2:
        removed = splice2Result(array, start, deleteCount, items[0], items[1]);
        break;
    default:
        return;
    }
    shouldBeArray(array, expected.array);
    shouldBeArray(removed, expected.removed);
}
noDFG(runCase);
noFTL(runCase);

var cases = [];
function addCase(values, start, deleteCount, items) {
    cases.push({ values, start, deleteCount, items, expected: referenceSplice(values, start, deleteCount, items) });
}

var ints = [1, 2, 3, 4, 5, 6, 7, 8];
var doubles = [1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5];
var objects = ['a', {}, null, 'd', 5, 'f', 6.5, 'h'];
var holes = [1, , 3, , 5, 6, , 8];
var longer = [];
for (var i = 0; i < 40; ++i)
    longer.push(i);

for (var values of [ints, doubles, objects, holes, longer]) {
    addCase(values, 2, 3, []);
    addCase(values, 2, 3, [10]);
    addCase(values, 2, 1, [10, 11, 12]);
    addCase(values, 2, 2, [10, 11]);
    addCase(values, 0, values.length, []);
    addCase(values, 0, values.length, [10, 11]);
    addCase(values, 3, 0, []);
    addCase(values, 3, 0, [10, 11, 12]);
    addCase(values, values.length, 2, [10, 11, 12]);
    addCase(values, -2, 1, [10]);
    addCase(values, 1, -1, [10, 11]);
}
addCase(ints, 1, 1, [2.5, 'x', {}]);
addCase(ints, 1, 2, [0.5, 1]);
addCase(doubles, 1, 1, [NaN, 1, 2]);
addCase(doubles, 1, 1, [0, -0, 3]);
addCase(doubles, 1, 2, ['x', 1]);

// Configurations that force ArrayStorage or disable the double shape cannot produce these shapes.
if (!$vm.isHavingABadTime() && $vm.indexingMode(makeArray([0.5])) === "ArrayWithDouble") {
    shouldBe($vm.indexingMode(makeArray(ints)), "ArrayWithInt32");
    shouldBe($vm.indexingMode(makeArray(doubles)), "ArrayWithDouble");
    shouldBe($vm.indexingMode(makeArray(objects)), "ArrayWithContiguous");
    shouldBe($vm.indexingMode(makeArray(holes)), "ArrayWithInt32");
}

class MyArray extends Array { }

for (var i = 0; i < Math.ceil(testLoopCount / 20); ++i) {
    for (var testCase of cases)
        runCase(testCase);

    var literal = [1, 2, 3, 4];
    var mode = $vm.indexingMode(literal);
    splice0(literal, 1, 0);
    shouldBe($vm.indexingMode(literal), mode);
    splice0(literal, 4, 2);
    shouldBe($vm.indexingMode(literal), mode);
    shouldBeArray(literal, [1, 2, 3, 4]);

    literal = [1.5, 2.5, 3.5];
    shouldBeArray(splice0Result(literal, 0, 3), [1.5, 2.5, 3.5]);
    shouldBeArray(literal, []);

    var subclass = MyArray.from([1, 2, 3]);
    var result = splice2Result(subclass, 0, 1, 7, 8);
    shouldBe(result instanceof MyArray, true);
    shouldBeArray(result, [1]);
    shouldBeArray(subclass, [7, 8, 2, 3]);
}

function shouldBeHuge(array, length, prefix, suffixStart) {
    shouldBe(array.length, length);
    for (var i = 0; i < prefix.length; ++i)
        shouldBe(array[i], prefix[i]);
    shouldBe(array[prefix.length], suffixStart);
    shouldBe(array[length - 1], 119999);
}

var huge = [];
for (var i = 0; i < 120000; ++i)
    huge.push(i);
var array = huge.slice();
splice1(array, 5, 3, 10);
shouldBeHuge(array, 119998, [0, 1, 2, 3, 4, 10], 8);
splice3(array, 5, 1, 20, 21, 22);
shouldBeHuge(array, 120000, [0, 1, 2, 3, 4, 20, 21, 22], 8);
splice2(array, 5, 2, 30, 31);
shouldBeHuge(array, 120000, [0, 1, 2, 3, 4, 30, 31, 22], 8);
splice0(array, 119990, 5);
shouldBe(array.length, 119995);
shouldBe(array[119989], 119989);
shouldBe(array[119990], 119995);

var frozen = Object.freeze([1, 2, 3]);
var error = null;
try {
    splice3(frozen, 0, 1, 7, 8, 9);
} catch (e) {
    error = e;
}
shouldBe(error instanceof TypeError, true);
shouldBeArray(frozen, [1, 2, 3]);

Array.prototype[10] = 'proto';
array = makeArray([1, 2, 3, 4]);
splice1(array, 0, 2, 9);
shouldBeArray(array, [9, 3, 4]);
splice3(array, 1, 0, 5, 6, 7);
shouldBeArray(array, [9, 5, 6, 7, 3, 4]);
delete Array.prototype[10];
