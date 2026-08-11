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
                return [6, 7, 8].includes(key);
            };
        },
        get keys() {
            accessOrder.push('keys');
            return function() {
                let values = [6, 7, 8];
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
    
    let result = set.isDisjointFrom(obj);
    shouldBe(result, true);
    
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
                return [4, 5, 6, 7, 8].includes(key);
            };
        },
        get keys() {
            accessOrder.push('keys');
            return function() {
                let values = [4, 5, 6, 7, 8];
                let index = 0;
                return {
                    next: function() {
                        throw new Error("next() should not be called when iterating over smaller set");
                    }
                };
            };
        }
    };
    
    let result = smallSet.isDisjointFrom(obj);
    shouldBe(result, true);
    
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
                throw new Error("has() should not be called when iterating over other set");
            };
        },
        get keys() {
            accessOrder.push('keys');
            return function() {
                let values = [11, 12, 13];
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
    
    let result = largeSet.isDisjointFrom(obj);
    shouldBe(result, true);
    
    shouldBe(accessOrder.length, 3);
    shouldBe(accessOrder[0], 'size');
    shouldBe(accessOrder[1], 'has');
    shouldBe(accessOrder[2], 'keys');
}

testLargeSetOptimization();
