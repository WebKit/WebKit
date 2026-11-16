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

for (var i = 0; i < 1e4; ++i) {
    shouldBe(testJNEQ('hello', 'world'), 30);
    shouldBe(testJEQ('hello', 'world'), 42);
    shouldBe(testJNEQ('world', 'world'), 42);
    shouldBe(testJEQ('world', 'world'), 30);
    shouldBe(testJNEQ(20.5, 'world'), 30);
    shouldBe(testJEQ(20.5, 'world'), 42);

    shouldBe(testJNEQ(20.5, 21.3), 30);
    shouldBe(testJEQ(20.5, 21.3), 42);
    shouldBe(testJNEQ(20.5, 20.5), 42);
    shouldBe(testJEQ(20.5, 20.5), 30);

    shouldBe(testJNEQB(0, 0), 1);
    shouldBe(testJEQB(0, 1), 1);
    shouldBe(testJNEQB('hello', 'hello'), 1);
    shouldBe(testJEQB('hello', 'world'), 1);
    shouldBe(testJNEQB(20.4, 20.4), 1);
    shouldBe(testJEQB('hello', 20.4), 1);
    shouldBe(testJNEQB(0, -0), 1);

    shouldBe(testJNEQF(0, 0), 0);
    shouldBe(testJEQF(0, 1), 0);
    shouldBe(testJNEQF(20.4, 20.4), 0);
    shouldBe(testJEQF(20.4, 10.5), 0);
    shouldBe(testJNEQF(0, -0), 0);
    shouldBe(testJEQF('hello', 10.5), 0);
    shouldBe(testJNEQF('hello', 'hello'), 0);
    shouldBe(testJEQF('hello', 'world'), 0);
}
