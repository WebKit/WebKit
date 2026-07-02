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
            return [6, 7, 8].includes(key);
        },
        keys: function() {
            let values = [6, 7, 8];
            let index = 0;
            return {
                next: function() {
                    iteratorCallCount++;
                    if (iteratorCallCount === 1) {
                        set1.delete(3);
                        set1.delete(4);
                        set1.add(9);
                    }
                    if (index < values.length) {
                        return { value: values[index++], done: false };
                    }
                    return { done: true };
                }
            };
        }
    };
    
    let result = set1.isDisjointFrom(otherObject);
    shouldBe(result, true);
    shouldBe(iteratorCallCount >= 1, true);
}

main();

function testIteratorModification() {
    let set1 = new Set([1, 2, 3, 4, 5]);
    let callCount = 0;
    
    let otherObject = {
        size: 2,
        values: [6, 7],
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
                        self.values = [6, 7];
                    }
                    if (index < self.values.length) {
                        return { value: self.values[index++], done: false };
                    }
                    return { done: true };
                }
            };
        }
    };
    
    let result = set1.isDisjointFrom(otherObject);
    shouldBe(result, true);
}

testIteratorModification();

function testSetModificationDuringIteration() {
    let set1 = new Set([1, 2, 3]);
    let iteratorCallCount = 0;
    
    let otherObject = {
        size: 2,
        has: function(key) {
            return [4, 5].includes(key);
        },
        keys: function() {
            let values = [4, 5];
            let index = 0;
            return {
                next: function() {
                    iteratorCallCount++;
                    if (iteratorCallCount === 1) {
                        set1.add(4);
                    }
                    if (index < values.length) {
                        return { value: values[index++], done: false };
                    }
                    return { done: true };
                }
            };
        }
    };
    
    let result = set1.isDisjointFrom(otherObject);
    shouldBe(result, false);
}

testSetModificationDuringIteration();
