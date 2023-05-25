function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test1() {
    return "li".toLowerCase();
}
noInline(test1);

function test2() {
    return "LI".toLowerCase();
}
noInline(test2);

for (var i = 0; i < 1e6; ++i) {
    shouldBe(test1(), "li");
    shouldBe(test2(), "li");
}
