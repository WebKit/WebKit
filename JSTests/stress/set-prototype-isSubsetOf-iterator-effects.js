function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function main() {
    let set1 = new Set([1, 2, 3, 4, 5]);
    let hasCallCount = 0;
    
    let otherObject = {
        size: 10,
        has: function(key) {
            hasCallCount++;
            if (hasCallCount === 1) {
                set1.delete(3);
                set1.delete(4);
                set1.add(6);
            }
            return [1, 2, 3, 4, 5, 6].includes(key);
        },
        keys: function() {
            return {
                next: function() {
                    throw new Error("keys() iterator should not be used in isSubsetOf");
                }
            };
        }
    };
    
    let result = set1.isSubsetOf(otherObject);
    shouldBe(hasCallCount >= 1, true);
}

main();

function testHasModification() {
    let set1 = new Set([1, 2]);
    let callCount = 0;
    
    let otherObject = {
        size: 5,
        values: [1, 2, 3, 4, 5],
        has: function(key) {
            callCount++;
            if (callCount === 1) {
                this.values = [1, 2];
            }
            return this.values.includes(key);
        },
        keys: function() {
            return {
                next: function() {
                    throw new Error("keys() should not be called");
                }
            };
        }
    };
    
    let result = set1.isSubsetOf(otherObject);
    shouldBe(result, true);
}

testHasModification();
