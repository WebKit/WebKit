function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

let set = new Set([0, -0, NaN, undefined, null, "", false, true, 42]);

let setLike = {
    size: 6,
    has: function(key) {
        if (key === 0 || key === -0) return true;
        if (Number.isNaN(key)) return true;
        if (key === undefined) return true;
        if (key === "new") return true;
        if (key === 100) return true;
        return false;
    },
    keys: function() {
        let values = [0, NaN, undefined, "new", 100];
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

let result = set.symmetricDifference(setLike);

shouldBe(result.has(0), false);
shouldBe(result.has(NaN), false);
shouldBe(result.has(undefined), false);

shouldBe(result.has(null), true);
shouldBe(result.has(""), true);
shouldBe(result.has(false), true);
shouldBe(result.has(true), true);
shouldBe(result.has(42), true);

shouldBe(result.has("new"), true);
shouldBe(result.has(100), true);

let sym1 = Symbol('test1');
let sym2 = Symbol('test2');
let sym3 = Symbol('test3');
let symbolSet = new Set([sym1, sym2]);

let symbolSetLike = {
    size: 2,
    has: function(key) {
        return key === sym2 || key === sym3;
    },
    keys: function() {
        let values = [sym2, sym3];
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

result = symbolSet.symmetricDifference(symbolSetLike);
shouldBe(result.has(sym1), true);
shouldBe(result.has(sym2), false);
shouldBe(result.has(sym3), true);

let obj1 = { a: 1 };
let obj2 = { b: 2 };
let obj3 = { c: 3 };
let objectSet = new Set([obj1, obj2]);

let objectSetLike = {
    size: 2,
    has: function(key) {
        return key === obj2 || key === obj3;
    },
    keys: function() {
        let values = [obj2, obj3];
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

result = objectSet.symmetricDifference(objectSetLike);
shouldBe(result.has(obj1), true);
shouldBe(result.has(obj2), false);
shouldBe(result.has(obj3), true);
