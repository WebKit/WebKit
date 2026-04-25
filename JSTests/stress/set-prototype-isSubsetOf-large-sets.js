function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function createLargeSet(size, start = 0) {
    let set = new Set();
    for (let i = start; i < start + size; i++) {
        set.add(i);
    }
    return set;
}

let largeSet = createLargeSet(10000, 0);
let smallSet = createLargeSet(100, 0);

let result = smallSet.isSubsetOf(largeSet);
shouldBe(result, true);

result = largeSet.isSubsetOf(smallSet);
shouldBe(result, false);

let set1 = createLargeSet(5000, 0);
let set2 = createLargeSet(5000, 2500);

result = set1.isSubsetOf(set2);
shouldBe(result, false);

result = set2.isSubsetOf(set1);
shouldBe(result, false);

let largeSet1 = createLargeSet(1000, 0);
let largeSet2 = createLargeSet(1000, 0);

result = largeSet1.isSubsetOf(largeSet2);
shouldBe(result, true);

result = largeSet2.isSubsetOf(largeSet1);
shouldBe(result, true);

let perfTestSet = createLargeSet(1000, 0);
let perfSetLike = {
    size: 2000,
    has: function(key) {
        return key >= 0 && key < 2000;
    },
    keys: function() {
        return {
            next: function() {
                throw new Error("keys() should not be called");
            }
        };
    }
};

result = perfTestSet.isSubsetOf(perfSetLike);
shouldBe(result, true);
