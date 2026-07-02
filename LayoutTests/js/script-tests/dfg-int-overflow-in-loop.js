description(
"Tests that overflowing an integer in a loop and then only using it in an integer context produces a result that complies with double arithmetic."
);

function foo(a) {
    var x = a;
    // Make sure that this is the loop where we do OSR entry.
    for (var i = 0; i < 100000; ++i)
        x += 1;
    // Now trigger overflow that is so severe that the floating point result would be different than the bigint result.
    for (var i = 0; i < 160097152; ++i)
        x += 2147483647;
    return x | 0;
}

shouldBe("foo(0)", "-4094336");

