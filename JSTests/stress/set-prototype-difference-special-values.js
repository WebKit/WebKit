function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function assertArrayContent(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < actual.length; i++) {
        if (!expected.includes(actual[i]))
            throw new Error(`unexpected element: ${actual[i]}`);
    }
    for (let i = 0; i < expected.length; i++) {
        if (!actual.includes(expected[i]))
            throw new Error(`missing element: ${expected[i]}`);
    }
}

let set = new Set([0, -0, NaN, undefined, null, "", false, true, 42, "extra"]);

let disjointSetLike = {
    size: 8,
    has: function(key) {
        if (key === 100) return true;
        if (key === "different") return true;
        if (key === false) return false;
        return [99, 98, 97, 96, 95, 94, 93].includes(key);
    },
    keys: function() {
        let values = [99, 98, 97, 96, 95, 94, 93, 100];
        let index = 0;
        return {
            next: function() {
                if (index < values.length) {
                    return { value: values[index++], done: false };
                }
                return { done: true };
            }
        };
    }
};

let result = set.difference(disjointSetLike);
assertArrayContent(Array.from(result), [0, NaN, undefined, null, "", false, true, 42, "extra"]);

let overlappingSetLike = {
    size: 8,
    has: function(key) {
        if (key === 0 || key === -0) return true;
        if (Number.isNaN(key)) return true;
        if (key === undefined) return true;
        if (key === null) return true;
        if (key === "") return true;
        if (key === false) return true;
        if (key === true) return true;
        if (key === 42) return true;
        return false;
    },
    keys: function() {
        let values = [0, NaN, undefined, null, "", false, true, 42];
        let index = 0;
        return {
            next: function() {
                if (index < values.length) {
                    return { value: values[index++], done: false };
                }
                return { done: true };
            }
        };
    }
};

result = set.difference(overlappingSetLike);
assertArrayContent(Array.from(result), ["extra"]);

let sym1 = Symbol('test1');
let sym2 = Symbol('test2');
let sym3 = Symbol('test3');
let symbolSet = new Set([sym1, sym2, sym3]);

let disjointSymbolSetLike = {
    size: 2,
    has: function(key) {
        return key === Symbol('test4') || key === Symbol('test5');
    },
    keys: function() {
        let values = [Symbol('test4'), Symbol('test5')];
        let index = 0;
        return {
            next: function() {
                if (index < values.length) {
                    return { value: values[index++], done: false };
                }
                return { done: true };
            }
        };
    }
};

result = symbolSet.difference(disjointSymbolSetLike);
assertArrayContent(Array.from(result), [sym1, sym2, sym3]);

let overlappingSymbolSetLike = {
    size: 2,
    has: function(key) {
        return key === sym1 || key === sym2;
    },
    keys: function() {
        let values = [sym1, sym2];
        let index = 0;
        return {
            next: function() {
                if (index < values.length) {
                    return { value: values[index++], done: false };
                }
                return { done: true };
            }
        };
    }
};

result = symbolSet.difference(overlappingSymbolSetLike);
assertArrayContent(Array.from(result), [sym3]);

let obj1 = { a: 1 };
let obj2 = { b: 2 };
let obj3 = { c: 3 };
let objectSet = new Set([obj1, obj2, obj3]);

let disjointObjectSetLike = {
    size: 2,
    has: function(key) {
        return key === { a: 1 } || key === { b: 2 };
    },
    keys: function() {
        let values = [{ a: 1 }, { b: 2 }];
        let index = 0;
        return {
            next: function() {
                if (index < values.length) {
                    return { value: values[index++], done: false };
                }
                return { done: true };
            }
        };
    }
};

result = objectSet.difference(disjointObjectSetLike);
assertArrayContent(Array.from(result), [obj1, obj2, obj3]);

let overlappingObjectSetLike = {
    size: 2,
    has: function(key) {
        return key === obj1 || key === obj2;
    },
    keys: function() {
        let values = [obj1, obj2];
        let index = 0;
        return {
            next: function() {
                if (index < values.length) {
                    return { value: values[index++], done: false };
                }
                return { done: true };
            }
        };
    }
};

result = objectSet.difference(overlappingObjectSetLike);
assertArrayContent(Array.from(result), [obj3]);
