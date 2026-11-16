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

let largeSet = createSequentialSet(10000, 0);
let smallSet = createSequentialSet(100, 5000);

for (let i = 0; i < 100; ++i) {
    let result = largeSet.union(smallSet);
    
    // Verify result size is correct
    shouldBe(result.size <= largeSet.size + smallSet.size, true);
    
    // Verify all elements from both sets are in result
    for (let value of largeSet) {
        shouldBe(result.has(value), true);
    }
    
    for (let value of smallSet) {
        shouldBe(result.has(value), true);
    }
}
