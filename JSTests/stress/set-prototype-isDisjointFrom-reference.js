function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function main() {
    let modificationDetected = false;
    const set = new Set([1, 2, 3, 4, 5]);

    try {
        let iteratorCallCount = 0;

        set.isDisjointFrom({
            size: 2,
            has: function(key) {
                return [6, 7].includes(key);
            },
            keys: function() {
                let values = [6, 7];
                let index = 0;
                return {
                    next: function() {
                        iteratorCallCount++;
                        
                        if (iteratorCallCount === 1) {
                            set.delete(1);
                            set.delete(2);
                            set.delete(3);
                            set.delete(4);
                            set.add(9);
                            modificationDetected = true;
                            if (index < values.length) {
                                return { value: values[index++], done: false };
                            }
                        } else if (iteratorCallCount === 2) {
                            throw new Error("Test exception");
                        }
                        
                        if (index < values.length) {
                            return { value: values[index++], done: false };
                        }
                        return { done: true };
                    }
                };
            }
        });
    } catch (e) {
        shouldBe(e.message, "Test exception");
    }

    shouldBe(modificationDetected, true);
    shouldBe(set.size, 2);
}

main();
