function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string) {
    return string.replace("Hello", "$`");
}
noInline(test);

function test2(string) {
    return string.replace("Hello", "REPLACE$`REPLACE");
}
noInline(test2);

for (var i = 0; i < 1e5; ++i)
    shouldBe(test("Hi, Hello World"), `Hi, Hi,  World`);

for (var i = 0; i < 1e5; ++i)
    shouldBe(test2("Hi, Hello World"), `Hi, REPLACEHi, REPLACE World`);
