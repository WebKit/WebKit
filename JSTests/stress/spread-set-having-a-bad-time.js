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

    const set = new Set([1, 2, 3]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, [1, 2, 3]);
    }
    Array.prototype[10] = "bad";
    try {
        for (let i = 0; i < 1e3; ++i) {
            const result = test(set);
            shouldBe(result.length, 3);
            shouldBe(result[0], 1);
            shouldBe(result[1], 2);
            shouldBe(result[2], 3);
        }
    } finally {
        delete Array.prototype[10];
    }
}

{
    function test(set) {
        return [...set];
    }
    noInline(test);
    const set = new Set([1, 2, 3]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBeArray(result, [1, 2, 3]);
    }
    Object.prototype[5] = "fromObject";
    try {
        for (let i = 0; i < 1e3; ++i) {
            const result = test(set);
            shouldBe(result.length, 3);
            shouldBeArray(result, [1, 2, 3]);
        }
    } finally {
        delete Object.prototype[5];
    }
}

{
    function test(set) {
        const arr = [...set];
        return arr;
    }
    noInline(test);
    const set = new Set([1, 2, 3]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBe(Object.getPrototypeOf(result), Array.prototype);
        shouldBe(result.constructor, Array);
        shouldBe(Array.isArray(result), true);
    }
}

{
    function test(set) {
        return [...set];
    }
    noInline(test);
    const set = new Set([0, 1, 2, 100, 1000]);
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBe(result.length, 5);
        shouldBeArray(result, [0, 1, 2, 100, 1000]);
    }
}

{
    function test(set) {
        return [...set];
    }
    noInline(test);
    const set = new Set([0, -0, 1]); // -0 should be treated same as 0
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBe(result.length, 2);
        shouldBe(result[0], 0);
        shouldBe(result[1], 1);
    }
}

{
    function test(set) {
        return [...set];
    }
    noInline(test);
    const set = new Set([NaN, NaN, 1]); // NaN should be treated as same value
    for (let i = 0; i < 1e4; ++i) {
        const result = test(set);
        shouldBe(result.length, 2);
        shouldBe(Number.isNaN(result[0]), true);
        shouldBe(result[1], 1);
    }
}
