function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function main() {
    let modificationDetected = false;
    const set = new Set([1, 2, 3, 4, 5]);
    let iteratorCallCount = 0;
    const result = set.symmetricDifference({
        size: 3,
        has: function(key) {
            return [3, 4, 6].includes(key);
        },
        keys: function() {
            let values = [3, 4, 6];
            let index = 0;
            return {
                next: function() {
                    iteratorCallCount++;
                    if (iteratorCallCount === 1) {
                        set.delete(1);
                        set.delete(2);
                        set.add(7);
                        modificationDetected = true;
                    }
                    if (index < values.length) {
                        return { value: values[index++], done: false };
                    }
                    return { done: true };
                }
            };
        }
    });

    shouldBe(modificationDetected, true);
    
    shouldBe(result.size, 4);
    shouldBe(result.has(1), true);
    shouldBe(result.has(2), true);
    shouldBe(result.has(3), false);
    shouldBe(result.has(4), false);
    shouldBe(result.has(5), true);
    shouldBe(result.has(6), true);
    shouldBe(result.has(7), false);    

    shouldBe(set.has(1), false);
    shouldBe(set.has(2), false);
    shouldBe(set.has(7), true);
}

function testIteratorModification() {
    let set = new Set([1, 2, 3]);
    let iteratorModified = false;
    
    let otherObject = {
        size: 2,
        has: function(key) {
            return [2, 4].includes(key);
        },
        keys: function() {
            let values = [2, 4];
            let index = 0;
            let iterator = {
                next: function() {
                    if (!iteratorModified && index === 1) {
                        iterator.modified = true;
                        iteratorModified = true;
                    }
                    
                    if (index < values.length) {
                        return { value: values[index++], done: false };
                    }
                    return { done: true };
                }
            };
            return iterator;
        }
    };

    let result = set.symmetricDifference(otherObject);
    
    shouldBe(iteratorModified, true);
    shouldBe(result.size, 3);
    shouldBe(result.has(1), true);
    shouldBe(result.has(2), false);
    shouldBe(result.has(3), true);
    shouldBe(result.has(4), true);
}

main();
testIteratorModification();
