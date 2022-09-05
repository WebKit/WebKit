function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test() {
    return "Hello".replace("Hey", "World");
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    shouldBe(test(), `Hello`);
