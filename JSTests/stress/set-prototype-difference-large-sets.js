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

let result = largeSet1.difference(largeSet2);
shouldBe(result.size, 10000);
for (let i = 0; i < 10000; i++) {
    shouldBe(result.has(i), true);
}
for (let i = 10000; i < 20000; i++) {
    shouldBe(result.has(i), false);
}

result = largeSet2.difference(largeSet1);
shouldBe(result.size, 10000);
for (let i = 0; i < 10000; i++) {
    shouldBe(result.has(i), false);
}
for (let i = 10000; i < 20000; i++) {
    shouldBe(result.has(i), true);
}

let overlappingSet1 = createLargeSet(5000, 0);
let overlappingSet2 = createLargeSet(5000, 2500);

result = overlappingSet1.difference(overlappingSet2);
shouldBe(result.size, 2500);
for (let i = 0; i < 2500; i++) {
    shouldBe(result.has(i), true);
}
for (let i = 2500; i < 7500; i++) {
    shouldBe(result.has(i), false);
}

result = overlappingSet2.difference(overlappingSet1);
shouldBe(result.size, 2500);
for (let i = 0; i < 2500; i++) {
    shouldBe(result.has(i), false);
}
for (let i = 5000; i < 7500; i++) {
    shouldBe(result.has(i), true);
}

let largeSet = createLargeSet(1000, 0);
let smallSet = createLargeSet(100, 1000);

result = largeSet.difference(smallSet);
shouldBe(result.size, 1000);
for (let i = 0; i < 1000; i++) {
    shouldBe(result.has(i), true);
}

result = smallSet.difference(largeSet);
shouldBe(result.size, 100);
for (let i = 1000; i < 1100; i++) {
    shouldBe(result.has(i), true);
}

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

result = perfTestSet.difference(perfSetLike);
shouldBe(result.size, 2000);
for (let i = 0; i < 2000; i++) {
    shouldBe(result.has(i), true);
}

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

result = perfTestSet.difference(overlappingPerfSetLike);
shouldBe(result.size, 1500);
for (let i = 0; i < 1500; i++) {
    shouldBe(result.has(i), true);
}
for (let i = 1500; i < 2000; i++) {
    shouldBe(result.has(i), false);
}
