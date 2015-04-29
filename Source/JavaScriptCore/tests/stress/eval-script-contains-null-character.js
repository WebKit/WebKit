function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual);
}

function test() {
    shouldBe(eval("(`\0`)"), "\0");
    shouldBe(eval("('\0')"), "\0");
}
noInline(test);

for (var i = 0; i < 10000; ++i)
    test();
