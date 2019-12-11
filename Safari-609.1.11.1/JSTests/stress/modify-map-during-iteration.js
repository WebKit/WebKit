
function testValue(value, expected) {
    if (value !== expected)
        throw new Error("bad value: expected:(" + expected + "),actual:(" + value +").");
}

function identityPairs(array) {
    return array.map(function (i) { return [i, i]; });
}

var map = new Map(identityPairs([0]));
var counter = 0;
for (var [elm, _] of map) {
    testValue(elm, counter);
    map.set(elm + 1, elm + 1);
    if (elm > 10000) {
        map.clear();
    }
    ++counter;
}
testValue(counter, 10002);

var map = new Map(identityPairs([0, 1, 2, 3]));
var counter = 0;
for (var [elm, _] of map) {
    testValue(elm, counter);
    map.clear();
    ++counter;
}
testValue(counter, 1);

var map = new Map(identityPairs([0, 1, 2, 3]));
var exp = [0, 2, 3];
var counter = 0;
for (var [elm, _] of map) {
    testValue(elm, exp[counter]);
    map.delete(counter + 1);
    ++counter;
}
testValue(counter, 3);

var map = new Map(identityPairs([0, 1, 2, 3]));
var iter = map.keys();
var iter2 = map.keys();
testValue(iter2.next().value, 0);

// Consume all output of iter.
for (var elm of iter);

testValue(iter.next().done, true);
testValue(iter.next().value, undefined);

map.clear();
map.set(1, 1).set(2, 2).set(3, 3);

testValue(iter.next().done, true);
testValue(iter.next().value, undefined);
testValue(iter2.next().value, 1);
testValue(iter2.next().value, 2);
testValue(iter2.next().value, 3);

var map = new Map();
map.set(1, 1);
map.delete(1);
map.forEach(function (i) {
    throw new Error("unreeachable.");
});

var map = new Map();
var iter = map[Symbol.iterator]();
map.set(1, 1);
map.delete(1);
for (var [elm, _] of map) {
    throw new Error("unreeachable.");
}

var map = new Map();
for (var i = 0; i < 5; ++i)
    map.set(i, i);
testValue(map.size, 5);
var iter = map.keys();
testValue(iter.next().value, 0);
testValue(iter.next().value, 1);
testValue(iter.next().value, 2);
testValue(iter.next().value, 3);
map.delete(0);
map.delete(1);
map.delete(2);
map.delete(3);
// It will cause MapData packing.
for (var i = 5; i < 1000; ++i)
    map.set(i, i);
gc();
for (var i = 4; i < 1000; ++i)
    testValue(iter.next().value, i);
testValue(iter.next().value, undefined);

