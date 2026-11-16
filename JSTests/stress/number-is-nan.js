function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}`);
}

function test1(i)
{
    shouldBe(Number.isNaN(i), false);
}
noInline(test1);

for (var i = -1e4; i < 1e4; ++i)
    test1(i);

function test2(i)
{
    shouldBe(Number.isNaN(Infinity), false);
    shouldBe(Number.isNaN(-Infinity), false);
    shouldBe(Number.isNaN(NaN), true);
}
noInline(test2);

// Emit DoubleRep.
for (var i = 0; i < 100; ++i)
    test2(i);


function test3(i)
{
    shouldBe(Number.isNaN("0"), false);
    shouldBe(Number.isNaN("Hello"), false);
    shouldBe(Number.isNaN("NaN"), false);
}
noInline(test3);

// Emit IsNumber.
for (var i = 0; i < 100; ++i)
    test3(i);
