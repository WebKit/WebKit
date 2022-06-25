function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + String(actual) + ' ' + String(expected));
}
noInline(shouldBe);

function zero()
{
    return 0;
}
noInline(zero);

function negativeZero()
{
    return -0;
}
noInline(negativeZero);

var object = {
    valueOf()
    {
        return -0;
    }
};

function test()
{
    shouldBe(0 < zero(), false);
    shouldBe(0 < (-zero()), false);
    shouldBe(0 <= zero(), true);
    shouldBe(0 <= (-zero()), true);
    shouldBe(0 > zero(), false);
    shouldBe(0 > (-zero()), false);
    shouldBe(0 >= zero(), true);
    shouldBe(0 >= (-zero()), true);
    shouldBe(0 == zero(), true);
    shouldBe(0 == (-zero()), true);
    shouldBe(0 === zero(), true);
    shouldBe(0 === (-zero()), true);
    shouldBe(0 != zero(), false);
    shouldBe(0 != (-zero()), false);
    shouldBe(0 !== zero(), false);
    shouldBe(0 !== (-zero()), false);

    shouldBe(0 < object, false);
    shouldBe(0 < -object, false);
    shouldBe(0 <= object, true);
    shouldBe(0 <= -object, true);
    shouldBe(0 > object, false);
    shouldBe(0 > -object, false);
    shouldBe(0 >= object, true);
    shouldBe(0 >= -object, true);
    shouldBe(0 == object, true);
    shouldBe(0 == -object, true);
    shouldBe(0 === object, false);
    shouldBe(0 === -object, true);
    shouldBe(0 != object, false);
    shouldBe(0 != -object, false);
    shouldBe(0 !== object, true);
    shouldBe(0 !== -object, false);
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test();
