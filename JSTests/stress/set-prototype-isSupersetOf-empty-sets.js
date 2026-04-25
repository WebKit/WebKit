function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

let emptySet1 = new Set();
let emptySet2 = new Set();
let result = emptySet1.isSupersetOf(emptySet2);
shouldBe(result, true);

let set1 = new Set([1, 2, 3]);
let empty = new Set();
result = empty.isSupersetOf(set1);
shouldBe(result, false);

result = set1.isSupersetOf(empty);
shouldBe(result, true);

let subset = new Set([1, 2]);
let superset = new Set([1, 2, 3, 4]);
result = subset.isSupersetOf(superset);
shouldBe(result, false);

result = superset.isSupersetOf(subset);
shouldBe(result, true);

let set2 = new Set([2, 3, 4]);
result = set1.isSupersetOf(set2);
shouldBe(result, false);

result = set2.isSupersetOf(set1);
shouldBe(result, false);

let set3 = new Set([1, 2, 3]);
result = set1.isSupersetOf(set3);
shouldBe(result, true);

result = set3.isSupersetOf(set1);
shouldBe(result, true);

let disjoint1 = new Set([1, 2, 3]);
let disjoint2 = new Set([4, 5, 6]);
result = disjoint1.isSupersetOf(disjoint2);
shouldBe(result, false);

result = disjoint2.isSupersetOf(disjoint1);
shouldBe(result, false);