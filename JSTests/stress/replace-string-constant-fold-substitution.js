function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test() {
    return "1Hello2".replace("Hello", "$`");
}
noInline(test);

function test2() {
    return "1Hello2".replace("Hello", "REPLACE$`REPLACE");
}
noInline(test2);

for (var i = 0; i < 1e5; ++i)
    shouldBe(test(), `112`);

for (var i = 0; i < 1e5; ++i)
    shouldBe(test2(), `1REPLACE1REPLACE2`);
