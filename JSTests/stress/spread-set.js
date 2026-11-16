function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Bad value: " + actual + " expected: " + expected);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

// Basic case: spread Set with integers
{
    function test(s) {
        return [...s];
    }
    noInline(test);

    const set = new Set([1, 2, 3]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, [1, 2, 3]);
    }
}

// Empty Set
{
    function test(s) {
        return [...s];
    }
    noInline(test);

    const set = new Set();
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, []);
    }
}

// Set with deleted elements
{
    function test(s) {
        return [...s];
    }
    noInline(test);

    const set = new Set([1, 2, 3, 4, 5]);
    set.delete(2);
    set.delete(4);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, [1, 3, 5]);
    }
}

// Large Set
{
    function test(s) {
        return [...s];
    }
    noInline(test);

    const arr = [];
    for (let i = 0; i < 1000; ++i)
        arr.push(i);
    const set = new Set(arr);

    for (let i = 0; i < 1e3; ++i) {
        const result = test(set);
        shouldBe(result.length, 1000);
        for (let j = 0; j < 1000; ++j)
            shouldBe(result[j], j);
    }
}

// Various types: double
{
    function test(s) {
        return [...s];
    }
    noInline(test);

    const set = new Set([1.5, 2.5, 3.5]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, [1.5, 2.5, 3.5]);
    }
}

// Various types: object
{
    function test(s) {
        return [...s];
    }
    noInline(test);

    const obj1 = {};
    const obj2 = {};
    const obj3 = {};
    const set = new Set([obj1, obj2, obj3]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBe(result.length, 3);
        shouldBe(result[0], obj1);
        shouldBe(result[1], obj2);
        shouldBe(result[2], obj3);
    }
}

// Various types: string
{
    function test(s) {
        return [...s];
    }
    noInline(test);

    const set = new Set(["a", "b", "c"]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, ["a", "b", "c"]);
    }
}

// Various types: symbol
{
    function test(s) {
        return [...s];
    }
    noInline(test);

    const sym1 = Symbol("a");
    const sym2 = Symbol("b");
    const sym3 = Symbol("c");
    const set = new Set([sym1, sym2, sym3]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBe(result.length, 3);
        shouldBe(result[0], sym1);
        shouldBe(result[1], sym2);
        shouldBe(result[2], sym3);
    }
}

// Various types: BigInt
{
    function test(s) {
        return [...s];
    }
    noInline(test);

    const set = new Set([1n, 2n, 3n]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, [1n, 2n, 3n]);
    }
}

// Insertion order preserved
{
    function test(s) {
        return [...s];
    }
    noInline(test);

    const set = new Set();
    set.add(3);
    set.add(1);
    set.add(4);
    set.add(1); // Duplicate, should not change order
    set.add(5);
    set.add(9);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, [3, 1, 4, 5, 9]);
    }
}

// Fallback when Symbol.iterator is modified
{
    function test(s) {
        return [...s];
    }
    noInline(test);

    const set = new Set([1, 2, 3]);

    // First, run normally to warm up
    for (let i = 0; i < 1e3; ++i) {
        const result = test(set);
        shouldBeArray(result, [1, 2, 3]);
    }

    // Modify Symbol.iterator
    const originalIterator = Set.prototype[Symbol.iterator];
    Set.prototype[Symbol.iterator] = function* () {
        yield 100;
        yield 200;
    };

    try {
        const result = test(set);
        shouldBeArray(result, [100, 200]);
    } finally {
        Set.prototype[Symbol.iterator] = originalIterator;
    }

    // Ensure it falls back correctly
    const result2 = test(set);
    shouldBeArray(result2, [1, 2, 3]);
}

// Fallback when SetIteratorPrototype.next is modified
{
    function test(s) {
        return [...s];
    }
    noInline(test);

    const set = new Set([1, 2, 3]);

    // First, run normally to warm up
    for (let i = 0; i < 1e3; ++i) {
        const result = test(set);
        shouldBeArray(result, [1, 2, 3]);
    }

    // Get SetIteratorPrototype and modify next
    const setIteratorPrototype = Object.getPrototypeOf(set[Symbol.iterator]());
    const originalNext = setIteratorPrototype.next;
    setIteratorPrototype.next = function() {
        const result = originalNext.call(this);
        if (!result.done)
            result.value = result.value * 10;
        return result;
    };

    try {
        const result = test(set);
        shouldBeArray(result, [10, 20, 30]);
    } finally {
        setIteratorPrototype.next = originalNext;
    }

    // Ensure it falls back correctly
    const result2 = test(set);
    shouldBeArray(result2, [1, 2, 3]);
}

// Set subclass
{
    class MySet extends Set {
        constructor(iterable) {
            super(iterable);
        }
    }

    function test(s) {
        return [...s];
    }
    noInline(test);

    const set = new MySet([1, 2, 3]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, [1, 2, 3]);
    }
}

// Mixed types in one Set
{
    function test(s) {
        return [...s];
    }
    noInline(test);

    const obj = {};
    const sym = Symbol();
    const set = new Set([1, "two", 3.0, null, undefined, obj, sym, 100n]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBe(result.length, 8);
        shouldBe(result[0], 1);
        shouldBe(result[1], "two");
        shouldBe(result[2], 3.0);
        shouldBe(result[3], null);
        shouldBe(result[4], undefined);
        shouldBe(result[5], obj);
        shouldBe(result[6], sym);
        shouldBe(result[7], 100n);
    }
}

// Spread in array literal context
{
    function test(s) {
        return [0, ...s, 4];
    }
    noInline(test);

    const set = new Set([1, 2, 3]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, [0, 1, 2, 3, 4]);
    }
}

// Spread in function call context
{
    function sum(...args) {
        return args.reduce((a, b) => a + b, 0);
    }

    function test(s) {
        return sum(...s);
    }
    noInline(test);

    const set = new Set([1, 2, 3, 4, 5]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBe(result, 15);
    }
}
