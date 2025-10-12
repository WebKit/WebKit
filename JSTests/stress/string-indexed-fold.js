function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test0()
{
    return "A".charAt(0);
}
noInline(test0);

function test1()
{
    return "TEST".charCodeAt(3);
}
noInline(test1);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(test0(), "A");
    shouldBe(test1(), 84);
}
