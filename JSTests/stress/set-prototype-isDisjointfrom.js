//@ runDefault("--useSetMethods=1")

function assert(a, e, m) {
    if (a !== e)
        throw new Error(m);
}

function assertArrayContent(a, e) {
    assert(a.length, e.length, "Size of arrays doesn't match");
    for (var i = 0; i < a.length; i++)
        assert(a[i], e[i], "a[" + i + "] = " + a[i] + " but e[" + i + "] = " + e[i]);
}

let obj1 = { };
let array1 = [ ];

let set1 = new Set([1]);
let set2 = new Set([2, 1]);
let set3 = new Set([3, 1]);
let set4 = new Set([1, 2, 3]);
let set5 = new Set([obj1, array1, set1, 3, 1]);
let set6 = new Set([obj1, array1, set1]);
let map1 = new Map([["a", 1], ["b", 2], [obj1, array1]]);
let map2 = new Map([[3, 1], [1, 2]]);

assert(set1.isDisjointFrom(set2), false);
assert(set2.isDisjointFrom(set1), false);
assert(set1.isDisjointFrom(set3), false);
assert(set3.isDisjointFrom(set1), false);
assert(set3.isDisjointFrom(set2), false);
assert(set2.isDisjointFrom(set3), false);
assert(set4.isDisjointFrom(set3), false);
assert(set2.isDisjointFrom(set4), false);
assert(set2.isDisjointFrom(set5), false);
assert(set3.isDisjointFrom(set5), false);
assert(set5.isDisjointFrom(set3), false);

assert(set1.isDisjointFrom(set6), true);
assert(set6.isDisjointFrom(set1), true);
assert(set4.isDisjointFrom(set6), true);
assert(set6.isDisjointFrom(set4), true);

assert(set3.isDisjointFrom(map1), true);
assert(set3.isDisjointFrom(map2), false);

try {
    // Not an object
    set1.isDisjointFrom(1);
} catch (e) {
    if (e != "TypeError: Set.prototype.isDisjointFrom expects the first parameter to be an object")
        throw e;
}

try {
    set1.isDisjointFrom({ });
} catch (e) {
    if (e != "TypeError: Set.prototype.isDisjointFrom expects other.size to be a non-NaN number")
        throw e;
}

try {
    set1.isDisjointFrom({ size:NaN });
} catch (e) {
    if (e != "TypeError: Set.prototype.isDisjointFrom expects other.size to be a non-NaN number")
        throw e;
}

try {
    set1.isDisjointFrom({ size:1 });
} catch (e) {
    if (e != "TypeError: Set.prototype.isDisjointFrom expects other.has to be callable")
        throw e;
}

try {
    set1.isDisjointFrom({ size:1, has(v) { return v == 1; } });
} catch (e) {
    if (e != "TypeError: Set.prototype.isDisjointFrom expects other.keys to be callable")
        throw e;
}

let error = new Error();
try {
    set1.isDisjointFrom({ size:1, has(v) { return v == 1; }, keys() { throw error } });
} catch (e) {
    if (e != error)
        throw e;
}

assert(set1.isDisjointFrom({ size:1, has(v) { return set1.has(v); }, keys() { return set1.keys() } }), false);
assert(set4.isDisjointFrom({ size:1, has(v) { return set1.has(v); }, keys() { return set1.keys() } }), false);
assert(set6.isDisjointFrom({ size:1, has(v) { return set1.has(v); }, keys() { return set1.keys() } }), true);
