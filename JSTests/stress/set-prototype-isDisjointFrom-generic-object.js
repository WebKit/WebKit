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
        return [6, 7, 8].includes(key);
    },
    keys: function() {
        let values = [6, 7, 8];
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

let result = set1.isDisjointFrom(setLikeObject);
shouldBe(result, true);

let overlappingSetLike = {
    size: 3,
    has: function(key) {
        return [3, 4, 6].includes(key);
    },
    keys: function() {
        let values = [3, 4, 6];
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

result = set1.isDisjointFrom(overlappingSetLike);
shouldBe(result, false);

let largerSetLike = {
    size: 10,
    has: function(key) {
        return key >= 6 && key <= 15;
    },
    keys: function() {
        let index = 6;
        return {
            next: function() {
                if (index <= 15) {
                    return { value: index++, done: false };
                }
                return { done: true };
            }
        };
    }
};

let smallSet = new Set([1, 2, 3]);
result = smallSet.isDisjointFrom(largerSetLike);
shouldBe(result, true);

shouldThrow(() => {
    set1.isDisjointFrom({
        size: 3,
        has: "not a function",
        keys: function() { return {}; }
    });
}, TypeError);

shouldThrow(() => {
    set1.isDisjointFrom({
        size: 3,
        has: function() { return true; },
        keys: "not a function"
    });
}, TypeError);

shouldThrow(() => {
    set1.isDisjointFrom({
        size: NaN,
        has: function() { return true; },
        keys: function() { return {}; }
    });
}, TypeError);

shouldThrow(() => {
    set1.isDisjointFrom({
        size: -1,
        has: function() { return true; },
        keys: function() { return {}; }
    });
}, RangeError);
