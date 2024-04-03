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

assertArrayContent(Array.from(set1.intersection(set2)), [1]);
assertArrayContent(Array.from(set1.intersection(set3)), [1]);
assertArrayContent(Array.from(set2.intersection(set3)), [1]);
assertArrayContent(Array.from(set2.intersection(set4)), [2, 1]);
assertArrayContent(Array.from(set3.intersection(set4)), [3, 1]);
assertArrayContent(Array.from(set4.intersection(set2)), [2, 1]);
assertArrayContent(Array.from(set4.intersection(set3)), [3, 1]);
assertArrayContent(Array.from(set5.intersection(set3)), [3, 1]);
assertArrayContent(Array.from(set5.intersection(map1)), [obj1]);

try {
    // Not an object
    set1.intersection(1);
} catch (e) {
    if (e != "TypeError: Set operation expects first argument to be an object")
        throw e;
}

try {
    set1.intersection({ });
} catch (e) {
    if (e != "TypeError: Set operation expects first argument to have non-NaN 'size' property")
        throw e;
}

try {
    set1.intersection({ size:NaN });
} catch (e) {
    if (e != "TypeError: Set operation expects first argument to have non-NaN 'size' property")
        throw e;
}

try {
    set1.intersection({ size: -4.5 });
} catch (e) {
    if (e != "RangeError: Set operation expects first argument to have non-negative 'size' property")
        throw e;
}

try {
    set1.intersection({ size:1 });
} catch (e) {
    if (e != "TypeError: Set.prototype.intersection expects other.has to be callable")
        throw e;
}

try {
    set1.intersection({ size:1, has(v) { return v == 1; } });
} catch (e) {
    if (e != "TypeError: Set.prototype.intersection expects other.keys to be callable")
        throw e;
}

let error = new Error();
try {
    set1.intersection({ size:1, has(v) { return v == 1; }, keys() { throw error } });
} catch (e) {
    if (e != error)
        throw e;
}


assertArrayContent(Array.from(set1.intersection({ size:1, has(v) { return set1.has(v); }, keys() { return set1.keys() } })), [1]);
assertArrayContent(Array.from(set4.intersection({ size:1, has(v) { return set1.has(v); }, keys() { return set1.keys() } })), [1]);
