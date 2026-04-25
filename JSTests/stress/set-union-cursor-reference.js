function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ', expected: ' + expected);
}

function main() {
    let capturedKey = null;

    try {
        const set = new Set([1, 2, 3, 4, 5]);

        let iterationCount = 0;
        const result = set.union({
            size: 3,
            has: key => true,
            keys: function() {
                return {
                    next: function() {
                        iterationCount++;
                        
                        if (iterationCount === 1) {
                            // Modify the set during the generic path iteration
                            // This tests that our cursor handling is correct
                            set.delete(1);
                            set.delete(2);
                            set.delete(3);
                            set.delete(4);
                            return { value: 6, done: false };
                        } else if (iterationCount === 2) {
                            // Capture what remains in the original set
                            capturedKey = [...set][0]; // Should be 5
                            return { value: 7, done: false };
                        } else if (iterationCount === 3) {
                            return { value: 8, done: false };
                        } else {
                            return { done: true };
                        }
                    }
                };
            }
        });

        // Verify the result contains the original elements (due to efficient cloning)
        // plus the new elements from the iterator
        shouldBe(result.has(1), true); // From original set (cloned before modification)
        shouldBe(result.has(2), true); // From original set (cloned before modification)
        shouldBe(result.has(3), true); // From original set (cloned before modification)
        shouldBe(result.has(4), true); // From original set (cloned before modification)
        shouldBe(result.has(5), true); // From original set (cloned before modification)
        shouldBe(result.has(6), true); // From iterator
        shouldBe(result.has(7), true); // From iterator
        shouldBe(result.has(8), true); // From iterator
        shouldBe(result.size, 8);

        // The original set should be modified
        shouldBe(capturedKey, 5);
        shouldBe(set.size, 1);
        shouldBe(set.has(5), true);

    } catch (e) {
        throw new Error('Unexpected error: ' + e);
    }
}

main();
