//@ runDefault

// IRO can record an integer-range relationship for a value whose DFG node is not int32-typed.
// A for-of loop over an array gives the iterator index such a relationship while its node is a
// boxed Phi with an imprecise prediction. Validating that fact must reference the operand in its
// own representation and check the bound only when it is an int32 at runtime, never assuming a raw
// int32. This must run cleanly under --validateIntegerRangeOptimization.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " (expected " + expected + ")");
}

function sumOf(arr) {
    var sum = 0;
    for (var v of arr)
        sum += v;
    return sum;
}
noInline(sumOf);

var arr = [1, 2, 3, 4, 5];
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(sumOf(arr), 15);
