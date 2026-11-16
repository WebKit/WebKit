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

let result = largeSet.isSupersetOf(smallSet);
shouldBe(result, true);

result = smallSet.isSupersetOf(largeSet);
shouldBe(result, false);

let set1 = createLargeSet(5000, 0);
let set2 = createLargeSet(5000, 2500);

result = set1.isSupersetOf(set2);
shouldBe(result, false);

result = set2.isSupersetOf(set1);
shouldBe(result, false);

let largeSet1 = createLargeSet(1000, 0);
let largeSet2 = createLargeSet(1000, 0);

result = largeSet1.isSupersetOf(largeSet2);
shouldBe(result, true);

result = largeSet2.isSupersetOf(largeSet1);
shouldBe(result, true);

let perfTestSet = createLargeSet(2000, 0);
let perfSetLike = {
    size: 1000,
    has: function(key) {
        return key >= 0 && key < 1000;
    },
    keys: function() {
        let index = 0;
        return {
            next: function() {
                if (index < 1000) {
                    return { value: index++, done: false };
                }
                return { done: true };
            }
        };
    }
};

result = perfTestSet.isSupersetOf(perfSetLike);
shouldBe(result, true);