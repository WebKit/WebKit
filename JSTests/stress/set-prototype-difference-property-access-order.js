function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function main() {
    let accessOrder = [];
    let set = new Set([1, 2, 3, 4, 5]);
    
    let obj = {
        get size() {
            accessOrder.push('size');
            return 3;
        },
        get has() {
            accessOrder.push('has');
            return function(key) {
                return [2, 4].includes(key);
            };
        },
        get keys() {
            accessOrder.push('keys');
            return function() {
                let values = [2, 4, 6];
                let index = 0;
                return {
                    next: function() {
                        if (index < values.length) {
                            return { value: values[index++], done: false };
                        }
                        return { done: true };
                    }
                };
            };
        }
    };
    
    let result = set.difference(obj);
    shouldBe(result.has(1), true);
    shouldBe(result.has(2), false);
    shouldBe(result.has(3), true);
    shouldBe(result.has(4), false);
    shouldBe(result.has(5), true);
    
    shouldBe(accessOrder.length, 3);
    shouldBe(accessOrder[0], 'size');
    shouldBe(accessOrder[1], 'has');
    shouldBe(accessOrder[2], 'keys');
}

main();

function testSmallSetOptimization() {
    let accessOrder = [];
    let smallSet = new Set([1, 2, 3]);
    
    let obj = {
        get size() {
            accessOrder.push('size');
            return 5;
        },
        get has() {
            accessOrder.push('has');
            return function(key) {
                return [2, 4, 6, 7, 8].includes(key);
            };
        },
        get keys() {
            accessOrder.push('keys');
            return function() {
                let values = [2, 4, 6, 7, 8];
                let index = 0;
                return {
                    next: function() {
                        if (index < values.length) {
                            return { value: values[index++], done: false };
                        }
                        return { done: true };
                    }
                };
            };
        }
    };
    
    let result = smallSet.difference(obj);
    shouldBe(result.has(1), true);
    shouldBe(result.has(2), false);
    shouldBe(result.has(3), true);
    
    shouldBe(accessOrder.length, 3);
    shouldBe(accessOrder[0], 'size');
    shouldBe(accessOrder[1], 'has');
    shouldBe(accessOrder[2], 'keys');
}

testSmallSetOptimization();

function testLargeSetOptimization() {
    let accessOrder = [];
    let largeSet = new Set([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    
    let obj = {
        get size() {
            accessOrder.push('size');
            return 3;
        },
        get has() {
            accessOrder.push('has');
            return function(key) {
                return [2, 4, 6].includes(key);
            };
        },
        get keys() {
            accessOrder.push('keys');
            return function() {
                let values = [2, 4, 6];
                let index = 0;
                return {
                    next: function() {
                        if (index < values.length) {
                            return { value: values[index++], done: false };
                        }
                        return { done: true };
                    }
                };
            };
        }
    };
    
    let result = largeSet.difference(obj);
    shouldBe(result.has(1), true);
    shouldBe(result.has(2), false);
    shouldBe(result.has(3), true);
    shouldBe(result.has(4), false);
    shouldBe(result.has(5), true);
    shouldBe(result.has(6), false);
    shouldBe(result.has(7), true);
    shouldBe(result.has(8), true);
    shouldBe(result.has(9), true);
    shouldBe(result.has(10), true);
    
    shouldBe(accessOrder.length, 3);
    shouldBe(accessOrder[0], 'size');
    shouldBe(accessOrder[1], 'has');
    shouldBe(accessOrder[2], 'keys');
}

testLargeSetOptimization();
