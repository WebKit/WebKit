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
let map1 = new Map([["a", 1], ["b", 2], [obj1, array1]]);

assertArrayContent(Array.from(set1.symmetricDifference(set2)), [2]);
assertArrayContent(Array.from(set1.symmetricDifference(set3)), [3]);
assertArrayContent(Array.from(set2.symmetricDifference(set3)), [2, 3]);
assertArrayContent(Array.from(set2.symmetricDifference(set4)), [3]);
assertArrayContent(Array.from(set3.symmetricDifference(set4)), [2]);
assertArrayContent(Array.from(set5.symmetricDifference(set3)), [obj1, array1, set1]);
assertArrayContent(Array.from(set5.symmetricDifference(map1)), [array1, set1, 3, 1, "a", "b"]);

try {
    // Not an object
    set1.symmetricDifference(1);
} catch (e) {
    if (e != "TypeError: Set.prototype.symmetricDifference expects the first parameter to be an object")
        throw e;
}

try {
    set1.symmetricDifference({ });
} catch (e) {
    if (e != "TypeError: Set.prototype.symmetricDifference expects other.size to be a non-NaN number")
        throw e;
}

try {
    set1.symmetricDifference({ size:NaN });
} catch (e) {
    if (e != "TypeError: Set.prototype.symmetricDifference expects other.size to be a non-NaN number")
        throw e;
}

try {
    set1.symmetricDifference({ size:1 });
} catch (e) {
    if (e != "TypeError: Set.prototype.symmetricDifference expects other.has to be callable")
        throw e;
}

try {
    set1.symmetricDifference({ size:1, has(v) { return v == 1; } });
} catch (e) {
    if (e != "TypeError: Set.prototype.symmetricDifference expects other.keys to be callable")
        throw e;
}

let error = new Error();
try {
    set1.symmetricDifference({ size:1, has(v) { return v == 1; }, keys() { throw error } });
} catch (e) {
    if (e != error)
        throw e;
}


assertArrayContent(Array.from(set1.symmetricDifference({ size:1, has(v) { return set1.has(v); }, keys() { return set1.keys() } })), []);
assertArrayContent(Array.from(set4.symmetricDifference({ size:1, has(v) { return set1.has(v); }, keys() { return set1.keys() } })), [2, 3]);
