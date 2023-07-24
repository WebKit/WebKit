function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

for (var i = 0; i < 100; ++i) {
    let string = '';
    for (let i = 0; i < 3e4; ++i)
        string += String.fromCharCode(i & 0x7f);
    shouldBe(string.length, 3e4);
    shouldBe(string[30], String.fromCharCode(30 & 0x7f));
}
