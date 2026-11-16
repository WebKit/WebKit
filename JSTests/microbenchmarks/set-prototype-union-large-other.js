function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function createSequentialSet(size, start = 0) {
    let set = new Set();
    for (let i = start; i < start + size; i++) {
        set.add(i);
    }
    return set;
}

let smallSet = createSequentialSet(100, 0);
let largeSet = createSequentialSet(10000, 50);

for (let i = 0; i < 100; ++i) {
    let result = smallSet.union(largeSet);
    
    // Verify result size is correct
    shouldBe(result.size <= smallSet.size + largeSet.size, true);
    
    // Verify all elements from both sets are in result
    for (let value of smallSet) {
        shouldBe(result.has(value), true);
    }
    
    for (let value of largeSet) {
        shouldBe(result.has(value), true);
    }
}
