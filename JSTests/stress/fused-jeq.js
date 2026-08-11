function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)} ${String(expected)}`);
}

function testJNEQ(a, b)
{
    if (a == b) {
        return 42;
    }
    return 30;
}
noInline(testJNEQ);

function testJEQ(a, b)
{
    if (a != b) {
        return 42;
    }
    return 30;
}
noInline(testJEQ);

function testJNEQB(a, b)
{
    var i = 0;
    do {
        ++i;
    } while (!(a == b));
    return i;
}
noInline(testJNEQB);

function testJEQB(a, b)
{
    var i = 0;
    do {
        ++i;
    } while (!(a != b));
    return i;
}
noInline(testJEQB);

function testJNEQF(a, b)
{
    var i = 0;
    while (!(a == b))
        ++i;
    return i;
}
noInline(testJNEQF);

function testJEQF(a, b)
{
    var i = 0;
    while (!(a != b))
        ++i;
    return i;
}
noInline(testJEQF);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(testJNEQ(0, 42), 30);
    shouldBe(testJEQ(0, 42), 42);
    shouldBe(testJNEQB(0, 0), 1);
    shouldBe(testJEQB(0, 1), 1);
    shouldBe(testJNEQF(0, 0), 0);
    shouldBe(testJEQF(0, 1), 0);
}
