function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function main() {
    let emptyValue = null;

    try {
        const set = new Set([1, 2, 3, 4, 5]);

        let cnt = 0;
        set.union({
            size: 10,
            has: key => {
                return true;
            },
            keys: function() {
                let values = [6, 7, 8, 9, 10];
                let index = 0;
                return {
                    next: function() {
                        cnt++;
                        
                        if (cnt === 1) {
                            // Modify the original set during iteration
                            set.delete(1);
                            set.delete(2);
                            set.delete(3);
                            set.delete(4);
                        } else if (cnt === 2) {
                            emptyValue = set.size; // Should still see the original elements in result
                            throw 1; // Break out of iteration
                        }
                        
                        if (index < values.length) {
                            return { value: values[index++], done: false };
                        }
                        return { done: true };
                    }
                };
            }
        });
    } catch {
        // Expected to throw
    }

    // The result should contain the elements that were in the set at the time
    // the clone was made, not the current state of the set
    shouldBe(emptyValue, 1); // Only element 5 should remain in the original set
}

main();
