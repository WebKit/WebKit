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
                return true;
            };
        },
        get keys() {
            accessOrder.push('keys');
            return function() {
                let values = [1, 2, 3];
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
    
    set.isSupersetOf(obj);
    
    shouldBe(accessOrder.length, 3);
    shouldBe(accessOrder[0], 'size');
    shouldBe(accessOrder[1], 'has');
    shouldBe(accessOrder[2], 'keys');
}

main();

function testEarlyReturn() {
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
                throw new Error("has() should not be called due to size check");
            };
        },
        get keys() {
            accessOrder.push('keys');
            return function() {
                throw new Error("keys() should not be called due to size check");
            };
        }
    };
    
    let result = smallSet.isSupersetOf(obj);
    
    shouldBe(result, false);
    shouldBe(accessOrder.length, 3);
    shouldBe(accessOrder[0], 'size');
    shouldBe(accessOrder[1], 'has');
    shouldBe(accessOrder[2], 'keys');
}

testEarlyReturn();