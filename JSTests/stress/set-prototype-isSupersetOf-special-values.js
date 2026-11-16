function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

let set = new Set([0, -0, NaN, undefined, null, "", false, true, 42, "extra"]);

let setLike = {
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

let result = set.isSupersetOf(setLike);
shouldBe(result, true);

let sym1 = Symbol('test1');
let sym2 = Symbol('test2');
let sym3 = Symbol('test3');
let symbolSet = new Set([sym1, sym2, sym3]);

let symbolSetLike = {
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

result = symbolSet.isSupersetOf(symbolSetLike);
shouldBe(result, true);

let obj1 = { a: 1 };
let obj2 = { b: 2 };
let obj3 = { c: 3 };
let objectSet = new Set([obj1, obj2, obj3]);

let objectSetLike = {
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

result = objectSet.isSupersetOf(objectSetLike);
shouldBe(result, true);

let differentObj1 = { a: 1 };
let differentObj2 = { b: 2 };
let objectSetLikeDifferent = {
    size: 2,
    has: function(key) {
        return key === differentObj1 || key === differentObj2;
    },
    keys: function() {
        let values = [differentObj1, differentObj2];
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

result = objectSet.isSupersetOf(objectSetLikeDifferent);
shouldBe(result, false);