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

// Test that reusing a floating point value after use in a modulus works correctly.
function nonSpeculativeModReuseInner(argument, o1, o2)
{
 	// The + operator on objects is a reliable way to avoid the speculative JIT path for now at least.
    o1 + o2;

    var knownDouble = argument - 0;
    return knownDouble % 1 + knownDouble;
}
function nonSpeculativeModReuse(argument)
{
    return nonSpeculativeModReuseInner(argument, {}, {});
}

shouldBe("nonSpeculativeModReuse(0.5)", "1");
