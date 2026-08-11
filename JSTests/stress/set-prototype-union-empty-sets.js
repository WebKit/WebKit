function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

// Test with empty sets
let emptySet1 = new Set();
let emptySet2 = new Set();
let result = emptySet1.union(emptySet2);
shouldBe(result.size, 0);

// Test with one empty set
let set1 = new Set([1, 2, 3]);
let empty = new Set();
result = set1.union(empty);
shouldBe(result.size, 3);
shouldBe(result.has(1), true);
shouldBe(result.has(2), true);
shouldBe(result.has(3), true);

result = empty.union(set1);
shouldBe(result.size, 3);
shouldBe(result.has(1), true);
shouldBe(result.has(2), true);
shouldBe(result.has(3), true);

// Test with overlapping sets
let set2 = new Set([2, 3, 4]);
result = set1.union(set2);
shouldBe(result.size, 4);
shouldBe(result.has(1), true);
shouldBe(result.has(2), true);
shouldBe(result.has(3), true);
shouldBe(result.has(4), true);

// Test with identical sets
let set3 = new Set([1, 2, 3]);
result = set1.union(set3);
shouldBe(result.size, 3);
shouldBe(result.has(1), true);
shouldBe(result.has(2), true);
shouldBe(result.has(3), true);
