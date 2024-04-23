function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test1(a) {
    if (a !== true)
        return true;
    return false;
}
noInline(test1);

function test2(a) {
    if (a !== false)
        return true;
    return false;
}
noInline(test2);

function test3(a) {
    if(a !== null)
        return true;
    return false;
}
noInline(test3);

function test4(a) {
    if (a !== void 0)
        return true;
    return false;
}
noInline(test4);

for (var i = 0; i < 1e6; ++i) {
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
