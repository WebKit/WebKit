function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function toHighSurrogate(code)
{
    return (code >> 10) + (0xD800 - (0x10000 >> 10));
}

function toLowSurrogate(code)
{
    return (code & ((1 << 10) - 1)) + 0xDC00;
}

for (var i = 0x10000; i < 0x10ffff; ++i) {
    var high = toHighSurrogate(i);
    var low = toLowSurrogate(i);
    var str = String.fromCharCode(high, low);
    shouldBe(unescape(escape(str)), str);
}
