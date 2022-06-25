function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)} ${String(expected)}`);
}

function testJNSTRICTEQ(a, b)
{
    if (a === b) {
        return 42;
    }
    return 30;
}
noInline(testJNSTRICTEQ);

function testJSTRICTEQ(a, b)
{
    if (a !== b) {
        return 42;
    }
    return 30;
}
noInline(testJSTRICTEQ);

function testJNSTRICTEQB(a, b)
{
    var i = 0;
    do {
        ++i;
    } while (!(a === b));
    return i;
}
noInline(testJNSTRICTEQB);

function testJSTRICTEQB(a, b)
{
    var i = 0;
    do {
        ++i;
    } while (!(a !== b));
    return i;
}
noInline(testJSTRICTEQB);

function testJNSTRICTEQF(a, b)
{
    var i = 0;
    while (!(a === b))
        ++i;
    return i;
}
noInline(testJNSTRICTEQF);

function testJSTRICTEQF(a, b)
{
    var i = 0;
    while (!(a !== b))
        ++i;
    return i;
}
noInline(testJSTRICTEQF);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(testJNSTRICTEQ(0, 42), 30);
    shouldBe(testJSTRICTEQ(0, 42), 42);
    shouldBe(testJNSTRICTEQB(0, 0), 1);
    shouldBe(testJSTRICTEQB(0, 1), 1);
    shouldBe(testJNSTRICTEQF(0, 0), 0);
    shouldBe(testJSTRICTEQF(0, 1), 0);
}
