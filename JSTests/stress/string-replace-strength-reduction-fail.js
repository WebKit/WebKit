function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test() {
    return "Hello".replace("Hey", "World");
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(test(), `Hello`);
