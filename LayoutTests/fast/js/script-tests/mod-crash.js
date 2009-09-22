description(
"This test checks that n % 0 doesn't crash with a floating-point exception."
);

shouldBe("2 % 0", "NaN");

var n = 2;
shouldBe("n % 0", "NaN");

function f()
{
    return 2 % 0;
}

shouldBe("f()", "NaN");

function g()
{
    var n = 2;
    return n % 0;
}

shouldBe("g()", "NaN");

var successfullyParsed = true;
