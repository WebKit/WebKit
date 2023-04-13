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

let set1 = new Set([2, 1]);
let set2 = new Set([1, 2, 3]);
let set3 = new Set([obj1, array1, set1, 3, 1]);
let map1 = new Map([["a", 1], ["b", 2], [obj1, array1]]);

assertArrayContent(Array.from(set1.difference(set2)), []);
assertArrayContent(Array.from(set2.difference(set1)), [3]);
assertArrayContent(Array.from(set3.difference(set1)), [obj1, array1, set1, 3]);
assertArrayContent(Array.from(set3.difference(set2)), [obj1, array1, set1]);
assertArrayContent(Array.from(set1.difference(set3)), [2]);
assertArrayContent(Array.from(set2.difference(set3)), [2]);
assertArrayContent(Array.from(set3.difference(map1)), [array1, set1, 3, 1]);

try {
    // Not an object
    set1.difference(1);
} catch (e) {
    if (e != "TypeError: Set.prototype.difference expects the first parameter to be an object")
        throw e;
}

try {
    set1.difference({ });
} catch (e) {
    if (e != "TypeError: Set.prototype.difference expects other.size to be a non-NaN number")
        throw e;
}

try {
    set1.difference({ size: NaN });
} catch (e) {
    if (e != "TypeError: Set.prototype.difference expects other.size to be a non-NaN number")
        throw e;
}

try {
    set1.difference({ size: 1 });
} catch (e) {
    if (e != "TypeError: Set.prototype.difference expects other.has to be callable")
        throw e;
}

try {
    set1.difference({ size: 1, has(v) { return v == 1; } });
} catch (e) {
    if (e != "TypeError: Set.prototype.difference expects other.keys to be callable")
        throw e;
}

let error = new Error();
try {
    set1.difference({ size: 1, has(v) { return v == 1; }, keys() { throw error; } });
} catch (e) {
    if (e != error)
        throw e;
}

let s = new Set([1]);
assertArrayContent(Array.from(s.difference({ size:1, has(v) { return s.has(v); }, keys() { return s.keys() } })), []);
assertArrayContent(Array.from(set2.difference({ size:1, has(v) { return s.has(v); }, keys() { return s.keys() } })), [2, 3]);
