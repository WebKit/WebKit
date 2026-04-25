function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let set = new Set([1, 2, 3, 4, 5]);

let setLikeObject = {
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

let result = set.symmetricDifference(setLikeObject);
shouldBe(result.size, 4);
shouldBe(result.has(1), true);
shouldBe(result.has(2), true);
shouldBe(result.has(3), false);
shouldBe(result.has(4), false);
shouldBe(result.has(5), true);
shouldBe(result.has(6), true);

let partialMatch = {
    size: 10,
    has: function(key) {
        return key <= 3;
    },
    keys: function() {
        let values = [1, 2, 3, 7, 8];
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

result = set.symmetricDifference(partialMatch);
shouldBe(result.size, 4);
shouldBe(result.has(1), false);
shouldBe(result.has(2), false);
shouldBe(result.has(3), false);
shouldBe(result.has(4), true);
shouldBe(result.has(5), true);
shouldBe(result.has(7), true);
shouldBe(result.has(8), true);

function shouldThrow(func, expectedMessage) {
    try {
        func();
        throw new Error('Expected exception not thrown');
    } catch (e) {
        shouldBe(e.message, expectedMessage);
    }
}

shouldThrow(function() {
    set.symmetricDifference({
        size: 1,
        keys: function() { return { next: function() { return { done: true }; } }; }
    });
}, "Set.prototype.symmetricDifference expects other.has to be callable");

shouldThrow(function() {
    set.symmetricDifference({
        size: 1,
        has: "not a function",
        keys: function() { return { next: function() { return { done: true }; } }; }
    });
}, "Set.prototype.symmetricDifference expects other.has to be callable");

// Missing 'keys' method
shouldThrow(function() {
    set.symmetricDifference({
        size: 1,
        has: function() { return false; }
    });
}, "Set.prototype.symmetricDifference expects other.keys to be callable");

shouldThrow(function() {
    set.symmetricDifference({
        size: 1,
        has: function() { return false; },
        keys: "not a function"
    });
}, "Set.prototype.symmetricDifference expects other.keys to be callable");

shouldThrow(function() {
    set.symmetricDifference({
        size: 1,
        has: function() { return false; },
        keys: function() {
            return { next: "not a function" };
        }
    });
}, "Set.prototype.symmetricDifference expects other.keys().next to be callable");
