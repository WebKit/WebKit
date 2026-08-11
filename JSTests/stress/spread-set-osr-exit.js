function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Bad value: " + actual + " expected: " + expected);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

{
    function test(iterable) {
        return [...iterable];
    }
    noInline(test);
    const set = new Set([1, 2, 3]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, [1, 2, 3]);
    }
    const arr = [4, 5, 6];
    for (let i = 0; i < 100; ++i) {
        const result = test(arr);
        shouldBeArray(result, [4, 5, 6]);
    }
    for (let i = 0; i < 100; ++i) {
        const result = test(set);
        shouldBeArray(result, [1, 2, 3]);
    }
}

{
    function test(iterable) {
        return [...iterable];
    }
    noInline(test);
    const set = new Set([1, 2, 3]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, [1, 2, 3]);
    }
    const map = new Map([[1, "a"], [2, "b"]]);
    for (let i = 0; i < 100; ++i) {
        const result = test(map);
        shouldBe(result.length, 2);
        shouldBeArray(result[0], [1, "a"]);
        shouldBeArray(result[1], [2, "b"]);
    }
}

{
    function test(set) {
        return [...set];
    }
    noInline(test);
    const smallSet = new Set([1, 2, 3]);
    const largeSet = new Set();
    for (let i = 0; i < 1000; ++i)
        largeSet.add(i);
    for (let i = 0; i < 1e4; ++i) {
        if (i % 2 === 0) {
            const result = test(smallSet);
            shouldBe(result.length, 3);
        } else {
            const result = test(largeSet);
            shouldBe(result.length, 1000);
        }
    }
}

{
    function test(iterable) {
        try {
            return [...iterable];
        } catch (e) {
            return null;
        }
    }
    noInline(test);
    const set = new Set([1, 2, 3]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, [1, 2, 3]);
    }
    shouldBe(test(null), null);
    shouldBe(test(undefined), null);
    const result = test(set);
    shouldBeArray(result, [1, 2, 3]);
}

{
    function test(s1, s2) {
        return [...s1, ...s2];
    }
    noInline(test);
    const set1 = new Set([1, 2, 3]);
    const set2 = new Set([4, 5, 6]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set1, set2);
        shouldBeArray(result, [1, 2, 3, 4, 5, 6]);
    }
}

{
    function* gen() {
        yield 7;
        yield 8;
        yield 9;
    }
    function test(iterable) {
        return [...iterable];
    }
    noInline(test);
    const set = new Set([1, 2, 3]);
    for (let i = 0; i < 1e4; ++i) {
        if (i % 100 === 0) {
            const result = test(gen());
            shouldBeArray(result, [7, 8, 9]);
        } else {
            const result = test(set);
            shouldBeArray(result, [1, 2, 3]);
        }
    }
}
