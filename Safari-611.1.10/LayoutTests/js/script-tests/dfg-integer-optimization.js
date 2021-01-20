description(
"This tests that integer addition optimizations in the DFG are not performed too overzealously."
);

function doAdd(a,b) {
    // The point of this test is to see if the DFG CSE's the second (a + b) against the first, after
    // optimizing the first to be an integer addition. The first one certainly is an integer addition,
    // but the second one isn't - it must either be an integer addition with overflow checking, or a
    // double addition.
    return {a:((a + b) | 0), b:(a + b)};
}

for (var i = 0; i < 1000; ++i) {
    // Create numbers big enough that we'll start seeing doubles only after about 200 invocations.
    var a = i * 1000 * 1000 * 10;
    var b = i * 1000 * 1000 * 10 + 1;
    var result = doAdd(a, b);
    
    // Use eval() for computing the correct result, to force execution to happen outside the DFG.
    shouldBe("result.a", "" + eval("((" + a + " + " + b + ") | 0)"))
    shouldBe("result.b", "" + eval(a + " + " + b))
}

