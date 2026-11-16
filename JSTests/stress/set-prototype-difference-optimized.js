function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function assertArrayContent(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < actual.length; i++) {
        if (!expected.includes(actual[i]))
            throw new Error(`unexpected element: ${actual[i]}`);
    }
    for (let i = 0; i < expected.length; i++) {
        if (!actual.includes(expected[i]))
            throw new Error(`missing element: ${expected[i]}`);
    }
}

// Test 1: Fast path with native Sets
let set1 = new Set([1, 2, 3, 4, 5]);
let set2 = new Set([3, 4, 5, 6, 7]);
let result = set1.difference(set2);
assertArrayContent(Array.from(result), [1, 2]);

let set3 = new Set([1, 2, 3]);
let set4 = new Set([4, 5, 6]);
result = set3.difference(set4);
assertArrayContent(Array.from(result), [1, 2, 3]);

// Test 2: Empty sets
let emptySet1 = new Set();
let emptySet2 = new Set();
result = emptySet1.difference(emptySet2);
shouldBe(result.size, 0);

result = set1.difference(emptySet1);
assertArrayContent(Array.from(result), [1, 2, 3, 4, 5]);

result = emptySet1.difference(set1);
shouldBe(result.size, 0);

// Test 3: Large sets
function createLargeSet(size, start = 0) {
    let set = new Set();
    for (let i = start; i < start + size; i++) {
        set.add(i);
    }
    return set;
}

let largeSet1 = createLargeSet(10000, 0);
let largeSet2 = createLargeSet(5000, 5000);
result = largeSet1.difference(largeSet2);
shouldBe(result.size, 5000);
for (let i = 0; i < 5000; i++) {
    shouldBe(result.has(i), true);
}
for (let i = 5000; i < 10000; i++) {
    shouldBe(result.has(i), false);
}

// Test 4: Set-like objects with small size (thisSet.size <= otherSize)
let smallSet = new Set([1, 2, 3]);
let largeSetLike = {
    size: 10,
    has: function(key) {
        return key === 2 || key === 4;
    },
    keys: function() {
        let values = [2, 4, 5, 6, 7, 8, 9, 10, 11, 12];
        let index = 0;
        return {
            next: function() {
                if (index < values.length) {
                    return { value: values[index++], done: false };
                }
                return { done: true };
            }
        };
    }
};

result = smallSet.difference(largeSetLike);
assertArrayContent(Array.from(result), [1, 3]);

// Test 5: Set-like objects with large size (thisSet.size > otherSize)
let bigSet = new Set([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
let smallSetLike = {
    size: 3,
    has: function(key) {
        return [2, 4, 6].includes(key);
    },
    keys: function() {
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
    }
};

result = bigSet.difference(smallSetLike);
assertArrayContent(Array.from(result), [1, 3, 5, 7, 8, 9, 10]);

// Test 6: Special values
let specialSet1 = new Set([0, -0, NaN, undefined, null, "", false, true, 42]);
let specialSet2 = new Set([0, NaN, null, true, 100]);
result = specialSet1.difference(specialSet2);
assertArrayContent(Array.from(result), [undefined, "", false, 42]);

// Test 7: Objects and symbols
let obj1 = { a: 1 };
let obj2 = { b: 2 };
let obj3 = { c: 3 };
let sym1 = Symbol('test1');
let sym2 = Symbol('test2');

let objectSet1 = new Set([obj1, obj2, obj3, sym1, sym2]);
let objectSet2 = new Set([obj2, sym1]);
result = objectSet1.difference(objectSet2);
assertArrayContent(Array.from(result), [obj1, obj3, sym2]);

// Test 8: Iterator modification during iteration
let modSet = new Set([1, 2, 3, 4, 5]);
let modSetLike = {
    size: 3,
    has: function(key) {
        return [2, 4, 6].includes(key);
    },
    keys: function() {
        let values = [2, 4, 6];
        let index = 0;
        return {
            next: function() {
                if (index === 1) {
                    // Modify the original set during iteration
                    modSet.add(10);
                    modSet.delete(1);
                }
                if (index < values.length) {
                    return { value: values[index++], done: false };
                }
                return { done: true };
            }
        };
    }
};

result = modSet.difference(modSetLike);
// The result should contain elements not in modSetLike (2, 4, 6)
// Note: 1 was deleted during iteration, 10 was added during iteration
// The result captures the state at the time of difference computation
shouldBe(result.has(1), true); // was in original set, not in modSetLike
shouldBe(result.has(3), true);
shouldBe(result.has(5), true);
shouldBe(result.has(10), false); // added after iteration started

// Test 9: Performance test with different sizes
let perfSet1 = createLargeSet(1000, 0);
let perfSet2 = createLargeSet(100, 500);
result = perfSet1.difference(perfSet2);
shouldBe(result.size, 900);

// Test 10: All elements removed
let allSet1 = new Set([1, 2, 3]);
let allSet2 = new Set([1, 2, 3, 4, 5]);
result = allSet1.difference(allSet2);
shouldBe(result.size, 0);

// Test 11: Property access order validation
let accessOrder = [];
let trackedSetLike = {
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

let propSet = new Set([1, 2, 3, 4, 5]);
result = propSet.difference(trackedSetLike);
shouldBe(accessOrder.length, 3);
shouldBe(accessOrder[0], 'size');
shouldBe(accessOrder[1], 'has');
shouldBe(accessOrder[2], 'keys');
