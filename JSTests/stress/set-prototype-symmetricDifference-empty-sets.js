function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let emptySet1 = new Set();
let emptySet2 = new Set();

let result1 = emptySet1.symmetricDifference(emptySet2);
shouldBe(result1.size, 0);

let nonEmptySet = new Set([1, 2, 3]);
let result2 = emptySet1.symmetricDifference(nonEmptySet);
shouldBe(result2.size, 3);
shouldBe(result2.has(1), true);
shouldBe(result2.has(2), true);
shouldBe(result2.has(3), true);

let result3 = nonEmptySet.symmetricDifference(emptySet1);
shouldBe(result3.size, 3);
shouldBe(result3.has(1), true);
shouldBe(result3.has(2), true);
shouldBe(result3.has(3), true);

let emptySetLike = {
    size: 0,
    has: function(key) {
        return false;
    },
    keys: function() {
        return {
            next: function() {
                return { done: true };
            }
        };
    }
};

let result4 = nonEmptySet.symmetricDifference(emptySetLike);
shouldBe(result4.size, 3);
shouldBe(result4.has(1), true);
shouldBe(result4.has(2), true);
shouldBe(result4.has(3), true);

let result5 = emptySet1.symmetricDifference(emptySetLike);
shouldBe(result5.size, 0);

let fakeSizeSetLike = {
    size: 5,
    has: function(key) {
        return false;
    },
    keys: function() {
        return {
            next: function() {
                return { done: true };
            }
        };
    }
};

let result6 = nonEmptySet.symmetricDifference(fakeSizeSetLike);
shouldBe(result6.size, 3);
shouldBe(result6.has(1), true);
shouldBe(result6.has(2), true);
shouldBe(result6.has(3), true);
