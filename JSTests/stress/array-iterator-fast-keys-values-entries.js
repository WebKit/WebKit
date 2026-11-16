// Tests for fast iteration of JSArrayIterator (arr.keys(), arr.values(), arr.entries()).

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function values(array) {
    var sum = 0;
    for (var v of array.values())
        sum += v;
    return sum;
}
noInline(values);

function keys(array) {
    var sum = 0;
    for (var k of array.keys())
        sum += k;
    return sum;
}
noInline(keys);

function entries(array) {
    var sum = 0;
    for (var [k, v] of array.entries())
        sum += k * 1000 + v;
    return sum;
}
noInline(entries);

// Mixed kinds in one function — must coexist on the same op_iterator_open / op_iterator_next.
function mixed(array) {
    var sum = 0;
    for (var v of array.values()) sum += v;
    for (var k of array.keys()) sum += k;
    for (var [k, v] of array.entries()) sum += k + v;
    for (var v of array) sum += v; // FastArray (plain) path still works.
    return sum;
}
noInline(mixed);

var array = [];
for (var i = 0; i < 10; ++i)
    array.push(i + 1);

var expectedValues = 55;       // 1+2+...+10
var expectedKeys = 45;         // 0+1+...+9
var expectedEntries = 0;       // sum k*1000 + v over (0,1)..(9,10)
for (var i = 0; i < 10; ++i)
    expectedEntries += i * 1000 + (i + 1);
var expectedMixed = expectedValues + expectedKeys + (45 + 55) + expectedValues;

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(values(array), expectedValues);
    shouldBe(keys(array), expectedKeys);
    shouldBe(entries(array), expectedEntries);
    shouldBe(mixed(array), expectedMixed);
}

// Replacing Array.prototype.values does NOT invalidate arrayIteratorProtocolWatchpointSet (which
// watches Array.prototype[Symbol.iterator] and ArrayIteratorPrototype.next). The fast path should
// keep working — but on the new iterator returned by the replaced .values, which iterates [42, 42].
function valuesAfterReplacingValues(array) {
    var sum = 0;
    for (var v of array.values())
        sum += v;
    return sum;
}
noInline(valuesAfterReplacingValues);

// Warm up first.
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(valuesAfterReplacingValues(array), expectedValues);

Array.prototype.values = function () { return [42, 42][Symbol.iterator](); };
shouldBe(valuesAfterReplacingValues(array), 84);

// Iterating the result of arr.keys() / arr.entries() (i.e. the iterator itself, not arr) — exercises
// the FastArrayKeys/Entries open paths where iterable IS the iterator.
function valuesOfIterator(array) {
    var iter = array[Symbol.iterator]();
    var sum = 0;
    for (var v of iter)
        sum += v;
    return sum;
}
noInline(valuesOfIterator);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(valuesOfIterator(array), expectedValues);
