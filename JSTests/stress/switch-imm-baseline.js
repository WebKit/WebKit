//@ runDefault("--useDFGJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(value) {
    switch (value) {
    case 0:
        return 1;
    case 1:
        return 2;
    case 2:
        return 3;
    case 3:
        return 4;
    case 5:
        return 6;
    }
    return 7;
}
noInline(test);

for (var i = 0; i < 1e3; ++i) {
    shouldBe(test(0), 1);
    shouldBe(test(1), 2);
    shouldBe(test(2), 3);
    shouldBe(test(3), 4);
    shouldBe(test(4), 7);
    shouldBe(test(5), 6);
    shouldBe(test("Hey"), 7);
    shouldBe(test(null), 7);
    shouldBe(test(undefined), 7);
    shouldBe(test({}), 7);
}
