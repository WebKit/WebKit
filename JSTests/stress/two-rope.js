function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let string0 = '';
    for (let i = 0; i < 1e5; ++i)
        string0 += String.fromCharCode(i & 0x7f);
    let string1 = '';
    for (let i = 0; i < 1e5; ++i)
        string1 += String.fromCharCode(i & 0x7f);
    let result = string0 + string1;
    shouldBe(result.length, 1e5 * 2);
    for (let i = 0; i < 1e5; ++i)
        shouldBe(result[i], String.fromCharCode(i & 0x7f));
    for (let i = 0; i < 1e5; ++i)
        shouldBe(result[i + 1e5], String.fromCharCode(i & 0x7f));
}
