function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let string = '';
    for (let i = 0; i < testLoopCount; ++i)
        string += String.fromCharCode(i & 0x7f);
    shouldBe(string.length, testLoopCount);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(string[i], String.fromCharCode(i & 0x7f));
}
