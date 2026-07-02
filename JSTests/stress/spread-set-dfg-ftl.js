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
    function test(set) {
        return [...set];
    }
    noInline(test);

    const set = new Set([1, 2, 3, 4, 5]);

    for (let i = 0; i < 1e5; ++i) {
        const result = test(set);
        shouldBe(result.length, 5);
        if (i % 1e4 === 0) {
            shouldBeArray(result, [1, 2, 3, 4, 5]);
        }
    }
}

{
    function spreadSet(set) {
        return [...set];
    }
    function test(set) {
        const arr = spreadSet(set);
        return arr.reduce((a, b) => a + b, 0);
    }
    noInline(test);
    const set = new Set([1, 2, 3, 4, 5]);
    for (let i = 0; i < 1e5; ++i) {
        const result = test(set);
        shouldBe(result, 15);
    }
}

{
    function test(set, n) {
        let sum = 0;
        for (let i = 0; i < n; ++i) {
            const arr = [...set];
            sum += arr[i % arr.length];
        }
        return sum;
    }
    noInline(test);
    const set = new Set([1, 2, 3]);
    for (let i = 0; i < 1e3; ++i) {
        const result = test(set, 99); // 99 iterations: 33*1 + 33*2 + 33*3 = 198
        shouldBe(result, 198);
    }
}

{
    function test(set) {
        return [...set];
    }
    noInline(test);
    const set = new Set([1]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBe(result.length, set.size);
        if (i % 100 === 0) {
            set.add(i);
        }
        if (i % 500 === 0 && set.size > 10) {
            const firstValue = set.values().next().value;
            set.delete(firstValue);
        }
    }
}

{
    function innerSpread(set) {
        return [...set];
    }
    function outerSpread(set) {
        const arr = innerSpread(set);
        return [...new Set(arr.map(x => x * 2))];
    }
    noInline(outerSpread);
    const set = new Set([1, 2, 3]);
    for (let i = 0; i < 1e4; ++i) {
        const result = outerSpread(set);
        shouldBeArray(result, [2, 4, 6]);
    }
}

{
    function test(iterable) {
        try {
            return [...iterable];
        } catch (e) {
            return [];
        }
    }
    noInline(test);
    const set = new Set([1, 2, 3]);
    for (let i = 0; i < 1e5; ++i) {
        const result = test(set);
        shouldBeArray(result, [1, 2, 3]);
    }
}

{
    function test(arr) {
        const set = new Set(arr);
        return [...set];
    }
    noInline(test);
    const arr = [1, 2, 3, 2, 1]; // Has duplicates
    for (let i = 0; i < 1e4; ++i) {
        const result = test(arr);
        shouldBeArray(result, [1, 2, 3]);
    }
}
