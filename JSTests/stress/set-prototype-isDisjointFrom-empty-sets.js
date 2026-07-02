function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

let emptySet1 = new Set();
let emptySet2 = new Set();
let result = emptySet1.isDisjointFrom(emptySet2);
shouldBe(result, true);

let set1 = new Set([1, 2, 3]);
let empty = new Set();
result = empty.isDisjointFrom(set1);
shouldBe(result, true);

result = set1.isDisjointFrom(empty);
shouldBe(result, true);

let disjoint1 = new Set([1, 2, 3]);
let disjoint2 = new Set([4, 5, 6]);
result = disjoint1.isDisjointFrom(disjoint2);
shouldBe(result, true);

result = disjoint2.isDisjointFrom(disjoint1);
shouldBe(result, true);

let overlapping1 = new Set([1, 2, 3]);
let overlapping2 = new Set([3, 4, 5]);
result = overlapping1.isDisjointFrom(overlapping2);
shouldBe(result, false);

result = overlapping2.isDisjointFrom(overlapping1);
shouldBe(result, false);

let identical1 = new Set([1, 2, 3]);
let identical2 = new Set([1, 2, 3]);
result = identical1.isDisjointFrom(identical2);
shouldBe(result, false);

result = identical2.isDisjointFrom(identical1);
shouldBe(result, false);
