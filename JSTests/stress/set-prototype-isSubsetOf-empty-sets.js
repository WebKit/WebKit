function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

let emptySet1 = new Set();
let emptySet2 = new Set();
let result = emptySet1.isSubsetOf(emptySet2);
shouldBe(result, true);

let set1 = new Set([1, 2, 3]);
let empty = new Set();
result = empty.isSubsetOf(set1);
shouldBe(result, true);

result = set1.isSubsetOf(empty);
shouldBe(result, false);

let subset = new Set([1, 2]);
let superset = new Set([1, 2, 3, 4]);
result = subset.isSubsetOf(superset);
shouldBe(result, true);

result = superset.isSubsetOf(subset);
shouldBe(result, false);

let set2 = new Set([2, 3, 4]);
result = set1.isSubsetOf(set2);
shouldBe(result, false);

result = set2.isSubsetOf(set1);
shouldBe(result, false);

let set3 = new Set([1, 2, 3]);
result = set1.isSubsetOf(set3);
shouldBe(result, true);

result = set3.isSubsetOf(set1);
shouldBe(result, true);

let disjoint1 = new Set([1, 2, 3]);
let disjoint2 = new Set([4, 5, 6]);
result = disjoint1.isSubsetOf(disjoint2);
shouldBe(result, false);

result = disjoint2.isSubsetOf(disjoint1);
shouldBe(result, false);
