function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

for (var i = 0; i < 256; ++i) {
    var ch = String.fromCharCode(i);
    shouldBe(decodeURIComponent(encodeURIComponent(ch)), ch);
    shouldBe(decodeURI(encodeURI(ch)), ch);
    shouldBe(unescape(escape(ch)), ch);
}
