function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function main() {
    let set1 = new Set([1, 2, 3, 4, 5]);
    let iteratorCallCount = 0;
    
    let otherObject = {
        size: 3,
        has: function(key) {
            return [1, 2, 6].includes(key);
        },
        keys: function() {
            let values = [1, 2, 6];
            let index = 0;
            return {
                next: function() {
                    iteratorCallCount++;
                    if (iteratorCallCount === 1) {
                        set1.delete(3);
                        set1.delete(4);
                        set1.add(6);
                    }
                    if (index < values.length) {
                        return { value: values[index++], done: false };
                    }
                    return { done: true };
                }
            };
        }
    };
    
    let result = set1.isSupersetOf(otherObject);
    shouldBe(iteratorCallCount >= 1, true);
}

main();

function testIteratorModification() {
    let set1 = new Set([1, 2, 3, 4, 5]);
    let callCount = 0;
    
    let otherObject = {
        size: 2,
        values: [1, 2],
        has: function(key) {
            return this.values.includes(key);
        },
        keys: function() {
            let self = this;
            let index = 0;
            return {
                next: function() {
                    callCount++;
                    if (callCount === 1) {
                        self.values = [1, 2];
                    }
                    if (index < self.values.length) {
                        return { value: self.values[index++], done: false };
                    }
                    return { done: true };
                }
            };
        }
    };
    
    let result = set1.isSupersetOf(otherObject);
    shouldBe(result, true);
}

testIteratorModification();