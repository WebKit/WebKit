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

// Test with generic set-like object
let set1 = new Set([1, 2, 3]);
let setLikeObject = {
    size: 2,
    has: function(key) {
        return key === 4 || key === 5;
    },
    keys: function() {
        let values = [4, 5];
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

let result = set1.union(setLikeObject);
shouldBe(result.size, 5);
shouldBe(result.has(1), true);
shouldBe(result.has(2), true);
shouldBe(result.has(3), true);
shouldBe(result.has(4), true);
shouldBe(result.has(5), true);

// Test error cases
shouldThrow(() => {
    set1.union({
        size: 1,
        has: "not a function",
        keys: function() { return {}; }
    });
}, TypeError);

shouldThrow(() => {
    set1.union({
        size: 1,
        has: function() { return true; },
        keys: "not a function"
    });
}, TypeError);

shouldThrow(() => {
    set1.union({
        size: NaN,
        has: function() { return true; },
        keys: function() { return {}; }
    });
}, TypeError);

shouldThrow(() => {
    set1.union({
        size: -1,
        has: function() { return true; },
        keys: function() { return {}; }
    });
}, RangeError);
