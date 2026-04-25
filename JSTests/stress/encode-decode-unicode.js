function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function isLowSurrogate(code)
{
    return code >= 0xDC00 && code <= 0xDFFF;
}

function isHighSurrogate(code)
{
    return code >= 0xD800 && code <= 0xDBFF;
}

function isSurrogate(code)
{
    return isLowSurrogate(code) || isHighSurrogate(code);
}

for (var i = 256; i < 0xffff; ++i) {
    if (isSurrogate(i))
        continue;
    var ch = String.fromCharCode(i);
    shouldBe(decodeURIComponent(encodeURIComponent(ch)), ch);
    shouldBe(decodeURI(encodeURI(ch)), ch);
    shouldBe(unescape(escape(ch)), ch);
}
