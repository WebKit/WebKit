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
let set4 = new Set([obj1, array1, set1]);
let map1 = new Map([["a", 1], ["b", 2]]);

assertArrayContent(Array.from(set1.union(set2)), [1, 2]);
assertArrayContent(Array.from(set1.union(set3)), [1, 3]);
assertArrayContent(Array.from(set2.union(set3)), [2, 1, 3]);
assertArrayContent(Array.from(set4.union(set3)), [obj1, array1, set1, 3, 1]);
assertArrayContent(Array.from(set4.union(map1)), [obj1, array1, set1, "a", "b"]);

try {
    // Not an object
    set1.union(1);
} catch (e) {
    if (e != "TypeError: Set.prototype.union expects the first parameter to be an object")
        throw e;
}

try {
    set1.union({ });
} catch (e) {
    if (e != "TypeError: Set.prototype.union expects other.size to be a non-NaN number")
        throw e;
}

try {
    set1.union({ size:NaN });
} catch (e) {
    if (e != "TypeError: Set.prototype.union expects other.size to be a non-NaN number")
        throw e;
}

try {
    set1.union({ size:1 });
} catch (e) {
    if (e != "TypeError: Set.prototype.union expects other.has to be callable")
        throw e;
}

try {
    set1.union({ size:1, has(v) { return v == 1; } });
} catch (e) {
    if (e != "TypeError: Set.prototype.union expects other.keys to be callable")
        throw e;
}

let error = new Error();
try {
    set1.union({ size:1, has(v) { return v == 1; }, keys() { throw error } });
} catch (e) {
    if (e != error)
        throw e;
}


assertArrayContent(Array.from(set1.union({ size:1, has(v) { return set1.has(v); }, keys() { return set1.keys() } })), [1]);
assertArrayContent(Array.from(set4.union({ size:1, has(v) { return set1.has(v); }, keys() { return set1.keys() } })), [obj1, array1, set1, 1]);
