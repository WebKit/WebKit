function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var flag = false;
function test(string) {
    if (flag)
        return string.codePointAt(0x7fffffff);
    return 42;
}
noInline(test);

flag = true;
for (var i = 0; i < 40; ++i) {
    shouldBe(test(""), undefined);
}

flag = false;
for (var i = 0; i < 1e6; ++i)
    shouldBe(test(""), 42);

flag = true;
for (var i = 0; i < 1e6; ++i)
    shouldBe(test(""), undefined);
