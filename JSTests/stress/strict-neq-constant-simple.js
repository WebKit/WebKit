function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test1(a) {
    return a !== true;
}
noInline(test1);

function test2(a) {
    return a !== false;
}
noInline(test2);

function test3(a) {
    return a !== null;
}
noInline(test3);

function test4(a) {
    return a !== void 0;
}
noInline(test4);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(test1(42), true);
    shouldBe(test1(true), false);
    shouldBe(test2(42), true);
    shouldBe(test2(false), false);
    shouldBe(test3(42), true);
    shouldBe(test3(undefined), true);
    shouldBe(test3(null), false);
    shouldBe(test4(42), true);
    shouldBe(test4(null), true);
    shouldBe(test4(undefined), false);
}
