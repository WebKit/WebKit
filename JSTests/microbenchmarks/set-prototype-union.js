function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function createRandomSet(size) {
    let set = new Set();
    for (let i = 0; i < size; i++) {
        set.add(Math.random() * 1000000 | 0);
    }
    return set;
}

let set1 = createRandomSet(1000);
let set2 = createRandomSet(1000);

for (let i = 0; i < 1000; ++i) {
    let result = set1.union(set2);
    
    // Verify result size is correct (should be <= sum of both sets)
    shouldBe(result.size <= set1.size + set2.size, true);
    
    // Verify all elements from set1 are in result
    for (let value of set1) {
        shouldBe(result.has(value), true);
    }
    
    // Verify all elements from set2 are in result
    for (let value of set2) {
        shouldBe(result.has(value), true);
    }
}
