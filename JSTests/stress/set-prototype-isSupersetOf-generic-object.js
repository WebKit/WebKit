function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function shouldThrow(func, errorType) {
    let threw = false;
    try {
        func();
    } catch (e) {
        threw = true;
        shouldBe(e instanceof errorType, true);
    }
    shouldBe(threw, true);
}

let set1 = new Set([1, 2, 3, 4, 5]);
let setLikeObject = {
    size: 3,
    has: function(key) {
        return [1, 2, 3].includes(key);
    },
    keys: function() {
        let values = [1, 2, 3];
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

let result = set1.isSupersetOf(setLikeObject);
shouldBe(result, true);

let partialMatch = {
    size: 3,
    has: function(key) {
        return [1, 2, 6].includes(key);
    },
    keys: function() {
        let values = [1, 2, 6];
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

result = set1.isSupersetOf(partialMatch);
shouldBe(result, false);

let tooBig = {
    size: 10,
    has: function(key) {
        return true;
    },
    keys: function() {
        return {
            next: function() {
                throw new Error("Should not be called due to size check");
            }
        };
    }
};

let smallSet = new Set([1, 2, 3]);
result = smallSet.isSupersetOf(tooBig);
shouldBe(result, false);

shouldThrow(() => {
    set1.isSupersetOf({
        size: 3,
        has: "not a function",
        keys: function() { return {}; }
    });
}, TypeError);

shouldThrow(() => {
    set1.isSupersetOf({
        size: 3,
        has: function() { return true; },
        keys: "not a function"
    });
}, TypeError);

shouldThrow(() => {
    set1.isSupersetOf({
        size: NaN,
        has: function() { return true; },
        keys: function() { return {}; }
    });
}, TypeError);

shouldThrow(() => {
    set1.isSupersetOf({
        size: -1,
        has: function() { return true; },
        keys: function() { return {}; }
    });
}, RangeError);