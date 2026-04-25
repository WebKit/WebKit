function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function main() {
    const set1 = new Set([1, 2, 3]);
    const set2 = new Set([3, 4, 5]);
    const result = set1.symmetricDifference(set2);
    
    shouldBe(result.size, 4);
    shouldBe(result.has(1), true);
    shouldBe(result.has(2), true);
    shouldBe(result.has(3), false);
    shouldBe(result.has(4), true);
    shouldBe(result.has(5), true);
    
    const emptySet = new Set();
    const nonEmptySet = new Set([1, 2, 3]);
    
    const result1 = emptySet.symmetricDifference(nonEmptySet);
    shouldBe(result1.size, 3);
    shouldBe(result1.has(1), true);
    shouldBe(result1.has(2), true);
    shouldBe(result1.has(3), true);
    
    const result2 = nonEmptySet.symmetricDifference(emptySet);
    shouldBe(result2.size, 3);
    shouldBe(result2.has(1), true);
    shouldBe(result2.has(2), true);
    shouldBe(result2.has(3), true);
    
    const setA = new Set([1, 2, 3]);
    const setB = new Set([1, 2, 3]);
    const result3 = setA.symmetricDifference(setB);
    shouldBe(result3.size, 0);
    
    let iteratorCallCount = 0;
    const result4 = set1.symmetricDifference({
        size: 2,
        has: function(key) {
            return [3, 4].includes(key);
        },
        keys: function() {
            let values = [3, 4];
            let index = 0;
            return {
                next: function() {
                    iteratorCallCount++;
                    if (index < values.length) {
                        return { value: values[index++], done: false };
                    }
                    return { done: true };
                }
            };
        }
    });
    
    shouldBe(result4.size, 3);
    shouldBe(result4.has(1), true);
    shouldBe(result4.has(2), true);
    shouldBe(result4.has(3), false);
    shouldBe(result4.has(4), true);
    shouldBe(iteratorCallCount, 3);
}

main();
