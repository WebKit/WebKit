function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}`);
}

function test1(i)
{
    shouldBe(Number.isFinite(i), true);
}
noInline(test1);

for (var i = -1e4; i < 1e4; ++i)
    test1(i);

function test2(i)
{
    shouldBe(Number.isFinite(Infinity), false);
    shouldBe(Number.isFinite(-Infinity), false);
    shouldBe(Number.isFinite(NaN), false);
}
noInline(test2);

// Emit DoubleRep.
for (var i = 0; i < 100; ++i)
    test2(i);


function test3(i)
{
    shouldBe(Number.isFinite("0"), false);
    shouldBe(Number.isFinite("Hello"), false);
}
noInline(test3);

// Emit IsNumber.
for (var i = 0; i < 100; ++i)
    test3(i);
