function test(str, start, end) {
    return String.prototype.substring.call(str, start, end);
}

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

// negative startIndex x no endIndex
{
    const str = "Hello, World";
    let result;
    for (let i = 0; i < testLoopCount; i++) {
        result = test(str, -5);
        shouldBe(result, "Hello, World");
    }
}

// negative startIndex x negative endIndex
{
    const str = "Hello, World";
    let result;
    for (let i = 0; i < testLoopCount; i++) {
        result = test(str, -5, -3);
        shouldBe(result, "");
    }
}

// startIndex > endIndex
{
    const str = "Hello, World";
    let result;
    for (let i = 0; i < testLoopCount; i++) {
        result = test(str, 5, 2);
        shouldBe(result, "llo");
    }
}

// NaN startIndex x NaN endIndex
{
    const str = "Hello, World";
    let result;
    for (let i = 0; i < testLoopCount; i++) {
        result = test(str, NaN, NaN);
        shouldBe(result, "");
    }
}
