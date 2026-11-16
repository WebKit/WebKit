function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let string0 = '';
    for (let i = 0; i < testLoopCount; ++i)
        string0 += String.fromCharCode(i);
    let string1 = '';
    for (let i = 0; i < testLoopCount; ++i)
        string1 += String.fromCharCode(i + testLoopCount);
    let result = string0 + string1;
    shouldBe(result.length, testLoopCount * 2);
    for (let i = 0; i < 2 * testLoopCount; ++i)
        shouldBe(result[i], String.fromCharCode(i));
}
