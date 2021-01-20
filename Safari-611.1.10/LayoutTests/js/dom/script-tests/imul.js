description("Test Math.imul behaviour");

shouldBe("Math.imul(1, 0.5)", "0");
shouldBe("Math.imul(1, -0.5)", "0");
shouldBe("Math.imul(2, 1<<30)", "-2147483648");
shouldBe("Math.imul(4, 1<<30)", "0");
shouldBe("Math.imul(1, NaN)", "0");
shouldBe("Math.imul(1, Infinity)", "0");

shouldThrow("Math.imul({valueOf:function(){throw 'left'}},{valueOf:function(){throw 'right'}})", "'left'");

shouldBe("Math.imul(0.5, 1)", "0");
shouldBe("Math.imul(-0.5, 1)", "0");
shouldBe("Math.imul(1<<30, 2)", "-2147483648");
shouldBe("Math.imul(1<<30, 4)", "0");
shouldBe("Math.imul(NaN, 1)", "0");
shouldBe("Math.imul(Infinity, 1)", "0");
shouldBe("Math.imul(NaN, NaN)", "0");
shouldBe("Math.imul(Infinity, Infinity)", "0");
shouldBe("Math.imul(Infinity, -Infinity)", "0");
shouldBe("Math.imul(-Infinity, Infinity)", "0");
shouldBe("Math.imul(-Infinity, -Infinity)", "0");
shouldBe("Math.imul(0xffffffff, 5)", "-5");

function testIMul(left, right, count)
{
    var result = 0;
    for (var i = 0; i < count; i++)
        result += Math.imul(left, right);
    return result;
}

shouldBe("testIMul(2,2,10000)", "40000")
shouldBe("testIMul(2.5,2,10000)", "40000")
shouldBe("testIMul(2,2.5,10000)", "40000")
shouldBe("testIMul(2.5,2.5,10000)", "40000")
shouldBe("testIMul(2.5,2.5,10000)", "40000")
shouldBe("testIMul(-2,-2,10000)", "40000")
shouldBe("testIMul(-2.5,-2,10000)", "40000")
shouldBe("testIMul(-2,-2.5,10000)", "40000")
shouldBe("testIMul(-2.5,-2.5,10000)", "40000")
shouldBe("testIMul(-2.5,-2.5,10000)", "40000")

shouldBe("testIMul(-2,2,10000)", "-40000")
shouldBe("testIMul(-2.5,2,10000)", "-40000")
shouldBe("testIMul(-2,2.5,10000)", "-40000")
shouldBe("testIMul(-2.5,2.5,10000)", "-40000")
shouldBe("testIMul(-2.5,2.5,10000)", "-40000")


shouldBe("testIMul(2,-2,10000)", "-40000")
shouldBe("testIMul(2.5,-2,10000)", "-40000")
shouldBe("testIMul(2,-2.5,10000)", "-40000")
shouldBe("testIMul(2.5,-2.5,10000)", "-40000")
shouldBe("testIMul(2.5,-2.5,10000)", "-40000")


shouldBe("testIMul(NaN, 1, 10000)", "0")
shouldBe("testIMul(Infinity, 1, 10000)", "0")
shouldBe("testIMul(1e40, 1, 10000)", "0")
shouldBe("testIMul(1, NaN, 10000)", "0")
shouldBe("testIMul(1, Infinity, 10000)", "0")
shouldBe("testIMul(1, 1e40, 10000)", "0")

successfullyParsed = true;
