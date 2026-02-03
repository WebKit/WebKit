function test(str, start, end) {
    return String.prototype.substring.call(str, start, end);
}

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

{
    const str = "Hello, World";
    let result;
    for (let i = 0; i < testLoopCount; i++) {
        result = test(str, 7, 8);
        shouldBe(result.length, 1);
        shouldBe(result, "W");
    }
}
