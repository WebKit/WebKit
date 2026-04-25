function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let string = '';
    for (let i = 0; i < testLoopCount; ++i)
        string += String.fromCharCode(i);
    shouldBe(string.length, testLoopCount);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(string[i], String.fromCharCode(i));
}
