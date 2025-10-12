function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

let set = new Set([0, -0, NaN, undefined, null, "", false, true]);

let setLike = {
    size: 10,
    has: function(key) {
        if (key === 0 || key === -0) return true;
        if (Number.isNaN(key)) return true;
        if (key === undefined) return true;
        if (key === null) return true;
        if (key === "") return true;
        if (key === false) return true;
        if (key === true) return true;
        return false;
    },
    keys: function() {
        return {
            next: function() {
                throw new Error("keys() should not be called");
            }
        };
    }
};

let result = set.isSubsetOf(setLike);
shouldBe(result, true);

let sym1 = Symbol('test1');
let sym2 = Symbol('test2');
let symbolSet = new Set([sym1, sym2]);

let symbolSetLike = {
    size: 3,
    has: function(key) {
        return key === sym1 || key === sym2;
    },
    keys: function() {
        return {
            next: function() {
                throw new Error("keys() should not be called");
            }
        };
    }
};

result = symbolSet.isSubsetOf(symbolSetLike);
shouldBe(result, true);

let obj1 = { a: 1 };
let obj2 = { b: 2 };
let objectSet = new Set([obj1, obj2]);

let objectSetLike = {
    size: 3,
    has: function(key) {
        return key === obj1 || key === obj2;
    },
    keys: function() {
        return {
            next: function() {
                throw new Error("keys() should not be called");
            }
        };
    }
};

result = objectSet.isSubsetOf(objectSetLike);
shouldBe(result, true);

let differentObj1 = { a: 1 };
let differentObj2 = { b: 2 };
let objectSetLikeDifferent = {
    size: 3,
    has: function(key) {
        return key === differentObj1 || key === differentObj2;
    },
    keys: function() {
        return {
            next: function() {
                throw new Error("keys() should not be called");
            }
        };
    }
};

result = objectSet.isSubsetOf(objectSetLikeDifferent);
shouldBe(result, false);
