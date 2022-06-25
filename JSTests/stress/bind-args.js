function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test(a, b, c, d, e)
{
    return a + b + c + d + e;
}
noInline(test);

function test2(a, b, c, d, e, f)
{
    return a + b + c + d + e + f;
}
noInline(test2);

for (var i = 0; i < 3e4; ++i) {
    shouldBe(test.bind(undefined)(1, 2, 3, 4, 5), 15);
    shouldBe(test.bind(undefined, 1)(2, 3, 4, 5), 15);
    shouldBe(test.bind(undefined, 2, 3)(4, 5, 6), 20);
    shouldBe(test.bind(undefined, 2, 3, 4)(5, 6), 20);
    shouldBe(test.bind(undefined, 2, 3, 4, 5)(6), 20);
    shouldBe(test.bind(undefined, 2, 3, 4, 5, 6)(), 20);

    shouldBe(test2.bind(undefined)(1, 1, 2, 3, 4, 5), 16);
    shouldBe(test2.bind(undefined, 1)(1, 2, 3, 4, 5), 16);
    shouldBe(test2.bind(undefined, 1, 1)(2, 3, 4, 5), 16);
    shouldBe(test2.bind(undefined, 1, 2, 3)(4, 5, 6), 21);
    shouldBe(test2.bind(undefined, 1, 2, 3, 4)(5, 6), 21);
    shouldBe(test2.bind(undefined, 1, 2, 3, 4, 5)(6), 21);
    shouldBe(test2.bind(undefined, 1, 2, 3, 4, 5, 6)(), 21);
}
