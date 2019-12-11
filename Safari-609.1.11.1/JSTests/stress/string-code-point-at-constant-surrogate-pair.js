function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string) {
    return string.codePointAt(0);
}
noInline(test);

function breaking(string) {
    return string.codePointAt(1);
}
noInline(breaking);

var string = "𠮷野家";
shouldBe(string.length, 4);
for (var i = 0; i < 1e6; ++i) {
    shouldBe(test(string), 0x20bb7);
    shouldBe(test("𠮷"), 0x20bb7);
    shouldBe(breaking(string), 0xdfb7);
}
