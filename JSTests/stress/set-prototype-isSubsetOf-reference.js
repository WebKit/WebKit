function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function main() {
    let modificationDetected = false;
    const set = new Set([1, 2, 3, 4, 5]);

    try {
        let hasCallCount = 0;

        set.isSubsetOf({
            size: 10,
            has: function(key) {
                hasCallCount++;
                
                if (hasCallCount === 1) {
                    set.delete(1);
                    set.delete(2);
                    set.delete(3);
                    set.delete(4);
                    modificationDetected = true;
                    return true;
                } else if (hasCallCount === 2) {
                    throw new Error("Test exception");
                }
                
                return true;
            },
            
            keys: function() {
                return {
                    next: function() {
                        throw new Error("keys() should not be called in isSubsetOf");
                    }
                };
            }
        });
    } catch (e) {
        shouldBe(e.message, "Test exception");
    }

    shouldBe(modificationDetected, true);
    shouldBe(set.size, 1);
}

main();
