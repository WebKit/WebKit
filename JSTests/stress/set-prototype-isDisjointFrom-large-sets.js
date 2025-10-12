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

let largeSet1 = createLargeSet(10000, 0);
let largeSet2 = createLargeSet(10000, 10000);

let result = largeSet1.isDisjointFrom(largeSet2);
shouldBe(result, true);

result = largeSet2.isDisjointFrom(largeSet1);
shouldBe(result, true);

let overlappingSet1 = createLargeSet(5000, 0);
let overlappingSet2 = createLargeSet(5000, 2500);

result = overlappingSet1.isDisjointFrom(overlappingSet2);
shouldBe(result, false);

result = overlappingSet2.isDisjointFrom(overlappingSet1);
shouldBe(result, false);

let largeSet = createLargeSet(1000, 0);
let smallSet = createLargeSet(100, 1000);

result = largeSet.isDisjointFrom(smallSet);
shouldBe(result, true);

result = smallSet.isDisjointFrom(largeSet);
shouldBe(result, true);

let perfTestSet = createLargeSet(2000, 0);
let perfSetLike = {
    size: 1000,
    has: function(key) {
        return key >= 2000 && key < 3000;
    },
    keys: function() {
        let index = 2000;
        return {
            next: function() {
                if (index < 3000) {
                    return { value: index++, done: false };
                }
                return { done: true };
            }
        };
    }
};

result = perfTestSet.isDisjointFrom(perfSetLike);
shouldBe(result, true);

let overlappingPerfSetLike = {
    size: 500,
    has: function(key) {
        return key >= 1500 && key < 2000;
    },
    keys: function() {
        let index = 1500;
        return {
            next: function() {
                if (index < 2000) {
                    return { value: index++, done: false };
                }
                return { done: true };
            }
        };
    }
};

result = perfTestSet.isDisjointFrom(overlappingPerfSetLike);
shouldBe(result, false);
