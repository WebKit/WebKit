description(
"This test checks corner cases of the number cell reuse code. In particular, it checks for known cases where code generation for number cell reuse caused assertions to fail."
);

function leftConstantRightSimple(a)
{
    return 0.1 * (a * a);
}

shouldBe("leftConstantRightSimple(2)", "0.4");

function leftConstantRightComplex(a)
{
    return 0.1 * (a * a + a);
}

shouldBe("leftConstantRightComplex(1)", "0.2");

function leftSimpleRightConstant(a)
{
    return (a * a) * 0.1;
}

shouldBe("leftSimpleRightConstant(2)", "0.4");

function leftComplexRightConstant(a)
{
    return (a * a + a) * 0.1;
}

shouldBe("leftComplexRightConstant(1)", "0.2");

function leftThisRightSimple(a)
{
    return this * (a * a);
}

shouldBeNaN("leftThisRightSimple(2)");
shouldBe("leftThisRightSimple.call(2, 2)", "8");

function leftThisRightComplex(a)
{
    return this * (a * a + a);
}

shouldBeNaN("leftThisRightComplex(2)");
shouldBe("leftThisRightComplex.call(2, 2)", "12");

function leftSimpleRightThis(a)
{
    return (a * a) * this;
}

shouldBeNaN("leftSimpleRightThis(2)");
shouldBe("leftSimpleRightThis.call(2, 2)", "8");

function leftComplexRightThis(a)
{
    return (a * a + a) * this;
}

shouldBeNaN("leftComplexRightThis(2)");
shouldBe("leftComplexRightThis.call(2, 2)", "12");

var successfullyParsed = true;
