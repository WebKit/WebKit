function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function assertArrayContent(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < actual.length; i++) {
        if (!expected.includes(actual[i]))
            throw new Error(`unexpected element: ${actual[i]}`);
    }
    for (let i = 0; i < expected.length; i++) {
        if (!actual.includes(expected[i]))
            throw new Error(`missing element: ${expected[i]}`);
    }
}

let emptySet1 = new Set();
let emptySet2 = new Set();
let result = emptySet1.difference(emptySet2);
shouldBe(result.size, 0);

let set1 = new Set([1, 2, 3]);
let empty = new Set();
result = empty.difference(set1);
shouldBe(result.size, 0);

result = set1.difference(empty);
assertArrayContent(Array.from(result), [1, 2, 3]);

let disjoint1 = new Set([1, 2, 3]);
let disjoint2 = new Set([4, 5, 6]);
result = disjoint1.difference(disjoint2);
assertArrayContent(Array.from(result), [1, 2, 3]);

result = disjoint2.difference(disjoint1);
assertArrayContent(Array.from(result), [4, 5, 6]);

let overlapping1 = new Set([1, 2, 3]);
let overlapping2 = new Set([3, 4, 5]);
result = overlapping1.difference(overlapping2);
assertArrayContent(Array.from(result), [1, 2]);

result = overlapping2.difference(overlapping1);
assertArrayContent(Array.from(result), [4, 5]);

let identical1 = new Set([1, 2, 3]);
let identical2 = new Set([1, 2, 3]);
result = identical1.difference(identical2);
shouldBe(result.size, 0);

result = identical2.difference(identical1);
shouldBe(result.size, 0);
