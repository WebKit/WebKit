function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function countThrows(func, string, errorType) {
    let count = 0;
    for (let i = 0; i < testLoopCount; ++i) {
        try {
            func(string);
        } catch (error) {
            if (!(error instanceof errorType))
                throw new Error("bad error: " + error);
            count++;
        }
    }
    return count;
}

let matching = "abcdef01-2345-4789-89ab-0123456789ab";

// lastIndex is already an object when the function gets compiled.
{
    let regExp = /^abc/;
    function testSmall(string) { return regExp.test(string); }
    noInline(testSmall);

    let calls = 0;
    regExp.lastIndex = { valueOf() { calls++; return 0; } };
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(testSmall("xyz"), false);
        shouldBe(testSmall("abcdef"), true);
    }
    shouldBe(calls, testLoopCount * 2);
}

// The patterns below are too large for RegExpTestInline, so RegExpTest stays in the graph.
{
    let regExp = /^(?:[a-f][0-9a-f]{7}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}|[a-f]{3,8}:\/\/[a-z0-9.-]+\.[a-z]{2,6}(?:\/[\w.-]*)*\/?(?:\?[\w=&]*)?)$/i;
    function testValueOf(string) { return regExp.test(string); }
    noInline(testValueOf);

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(testValueOf("xyz"), false);
        shouldBe(testValueOf(matching), true);
    }

    let calls = 0;
    regExp.lastIndex = { valueOf() { calls++; return 0; } };
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(testValueOf("xyz"), false);
        shouldBe(testValueOf(matching), true);
    }
    shouldBe(calls, testLoopCount * 2);
}

{
    let regExp = /^(?:[a-f][0-9a-f]{7}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}|[a-f]{3,8}:\/\/[a-z0-9.-]+\.[a-z]{2,6}(?:\/[\w.-]*)*\/?(?:\?[\w=&]*)?)$/i;
    function testSymbol(string) { return regExp.test(string); }
    noInline(testSymbol);

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(testSymbol("xyz"), false);
        shouldBe(testSymbol(matching), true);
    }

    regExp.lastIndex = Symbol();
    shouldBe(countThrows(testSymbol, "xyz", TypeError), testLoopCount);
}

{
    let regExp = /^(?:[a-f][0-9a-f]{7}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}|[a-f]{3,8}:\/\/[a-z0-9.-]+\.[a-z]{2,6}(?:\/[\w.-]*)*\/?(?:\?[\w=&]*)?)$/i;
    function testBigInt(string) { return regExp.test(string); }
    noInline(testBigInt);

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(testBigInt("xyz"), false);
        shouldBe(testBigInt(matching), true);
    }

    regExp.lastIndex = 1n;
    shouldBe(countThrows(testBigInt, "xyz", TypeError), testLoopCount);
}

{
    let regExp = /^(?:[a-f][0-9a-f]{7}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}|[a-f]{3,8}:\/\/[a-z0-9.-]+\.[a-z]{2,6}(?:\/[\w.-]*)*\/?(?:\?[\w=&]*)?)$/i;
    function testThrowingValueOf(string) { return regExp.test(string); }
    noInline(testThrowingValueOf);

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(testThrowingValueOf("xyz"), false);
        shouldBe(testThrowingValueOf(matching), true);
    }

    class LastIndexError extends Error { }
    regExp.lastIndex = { valueOf() { throw new LastIndexError; } };
    shouldBe(countThrows(testThrowingValueOf, "xyz", LastIndexError), testLoopCount);
}

{
    function testNewRegExp(string, lastIndex) {
        let regExp = /^abc/;
        regExp.lastIndex = lastIndex;
        return regExp.test(string);
    }
    noInline(testNewRegExp);

    let calls = 0;
    let lastIndex = { valueOf() { calls++; return 0; } };
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(testNewRegExp("xyz", lastIndex), false);
        shouldBe(testNewRegExp("abcdef", lastIndex), true);
    }
    shouldBe(calls, testLoopCount * 2);
}
