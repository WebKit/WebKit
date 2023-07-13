function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let string = '';
    for (let i = 0; i < 1e5; ++i)
        string += String.fromCharCode(i);
    shouldBe(string.length, 1e5);
    for (let i = 0; i < 1e5; ++i)
        shouldBe(string[i], String.fromCharCode(i));
}
