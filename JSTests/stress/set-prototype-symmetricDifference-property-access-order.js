function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function main() {
    let accessOrder = [];
    
    let obj = {
        get size() {
            accessOrder.push('size');
            return 2;
        },
        get has() {
            accessOrder.push('has');
            return function(key) {
                accessOrder.push('has-call-' + key);
                return [2, 3].includes(key);
            };
        },
        get keys() {
            accessOrder.push('keys');
            return function() {
                accessOrder.push('keys-call');
                let values = [2, 3];
                let index = 0;
                return {
                    get next() {
                        accessOrder.push('next');
                        return function() {
                            accessOrder.push('next-call');
                            if (index < values.length) {
                                return { value: values[index++], done: false };
                            }
                            return { done: true };
                        };
                    }
                };
            };
        }
    };

    let set = new Set([1, 2]);
    let result = set.symmetricDifference(obj);
    
    shouldBe(accessOrder[0], 'size');
    shouldBe(accessOrder[1], 'has');
    shouldBe(accessOrder[2], 'keys');
    shouldBe(accessOrder[3], 'keys-call');
    shouldBe(accessOrder[4], 'next');
    shouldBe(accessOrder[5], 'next-call');
    shouldBe(accessOrder[6], 'next-call');
    shouldBe(accessOrder[7], 'next-call');
    
    shouldBe(result.size, 2);
    shouldBe(result.has(1), true);
    shouldBe(result.has(2), false);
    shouldBe(result.has(3), true);
}

function testEarlyReturn() {
    let accessOrder = [];
    
    let obj = {
        get size() {
            accessOrder.push('size');
            return 1;
        },
        get has() {
            accessOrder.push('has');
            throw new Error("Test error in has");
        },
        get keys() {
            accessOrder.push('keys');
            return function() { return { next: function() { return { done: true }; } }; };
        }
    };

    let set = new Set([1]);
    
    try {
        set.symmetricDifference(obj);
        shouldBe(false, true);
    } catch (e) {
        shouldBe(e.message, "Test error in has");
        shouldBe(accessOrder.length, 2);
        shouldBe(accessOrder[0], 'size');
        shouldBe(accessOrder[1], 'has');
    }
}

main();
testEarlyReturn();
