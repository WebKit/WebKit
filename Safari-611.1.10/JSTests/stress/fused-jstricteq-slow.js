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
    shouldBe(testJNSTRICTEQ('hello', 'world'), 30);
    shouldBe(testJSTRICTEQ('hello', 'world'), 42);
    shouldBe(testJNSTRICTEQ('world', 'world'), 42);
    shouldBe(testJSTRICTEQ('world', 'world'), 30);
    shouldBe(testJNSTRICTEQ(20.5, 'world'), 30);
    shouldBe(testJSTRICTEQ(20.5, 'world'), 42);

    shouldBe(testJNSTRICTEQ(20.5, 21.3), 30);
    shouldBe(testJSTRICTEQ(20.5, 21.3), 42);
    shouldBe(testJNSTRICTEQ(20.5, 20.5), 42);
    shouldBe(testJSTRICTEQ(20.5, 20.5), 30);

    shouldBe(testJNSTRICTEQB(0, 0), 1);
    shouldBe(testJSTRICTEQB(0, 1), 1);
    shouldBe(testJNSTRICTEQB('hello', 'hello'), 1);
    shouldBe(testJSTRICTEQB('hello', 'world'), 1);
    shouldBe(testJNSTRICTEQB(20.4, 20.4), 1);
    shouldBe(testJSTRICTEQB('hello', 20.4), 1);
    shouldBe(testJNSTRICTEQB(0, -0), 1);

    shouldBe(testJNSTRICTEQF(0, 0), 0);
    shouldBe(testJSTRICTEQF(0, 1), 0);
    shouldBe(testJNSTRICTEQF(20.4, 20.4), 0);
    shouldBe(testJSTRICTEQF(20.4, 10.5), 0);
    shouldBe(testJNSTRICTEQF(0, -0), 0);
    shouldBe(testJSTRICTEQF('hello', 10.5), 0);
    shouldBe(testJNSTRICTEQF('hello', 'hello'), 0);
    shouldBe(testJSTRICTEQF('hello', 'world'), 0);
}
