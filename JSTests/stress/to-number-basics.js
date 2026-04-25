function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function test1(value)
{
    return Number(value) <= 42;
}
noInline(test1);

// Int32.
for (var i = 0; i < 1e4; ++i)
    shouldBe(test1(42), true);

// Doubles.
for (var i = 0; i < 1e4; ++i)
    shouldBe(test1(42.195), false);

// Non numbers.
for (var i = 0; i < 1e4; ++i)
    shouldBe(test1("Hello"), false);
