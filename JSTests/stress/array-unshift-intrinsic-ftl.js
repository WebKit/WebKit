//@ runFTLNoCJIT

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Bad value: " + actual + ", expected: " + expected);
}

function unshiftOnce(array, v) {
    return array.unshift(v);
}
noInline(unshiftOnce);

// Force-tier coverage of compileArrayUnshift for Int32, Double, and Contiguous,
// including the inline length-1 shift and the slow-path fallbacks.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var contig = ["a"];
        shouldBe(unshiftOnce(contig, "b"), 2);
        shouldBe(contig[0], "b");
        shouldBe(contig[1], "a");

        var ints = [42];
        shouldBe(unshiftOnce(ints, 7), 2);
        shouldBe(ints[0], 7);
        shouldBe(ints[1], 42);

        var doubles = [1.5];
        shouldBe(unshiftOnce(doubles, 2.5), 2);
        shouldBe(doubles[0], 2.5);
        shouldBe(doubles[1], 1.5);

        var emptyContig = [];
        shouldBe(unshiftOnce(emptyContig, "x"), 1);
        shouldBe(emptyContig[0], "x");

        // Length >= 2: slow path under FTL.
        var big = ["a", "b", "c"];
        shouldBe(unshiftOnce(big, "z"), 4);
        shouldBe(big[0], "z");
        shouldBe(big[3], "c");

        // Double hole: slow path under FTL.
        var holed = [1.5];
        delete holed[0];
        shouldBe(unshiftOnce(holed, 9.5), 2);
        shouldBe(holed[0], 9.5);
    }
})();

// Zero-arg unshift compiled under FTL must lower to a length read.
(function () {
    function unshiftNone(a) { return a.unshift(); }
    noInline(unshiftNone);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = ["a", "b", "c"];
        shouldBe(unshiftNone(array), 3);
        shouldBe(array.length, 3);
    }
})();

// FTL Double NaN speculation: warm up to FTL, then pass NaN once. The
// DoubleRepRealUse speculation in FTL must OSR exit (FTL_TYPE_CHECK on
// doubleNotEqualOrUnordered) and the slow path must produce a valid array.
(function () {
    function unshiftDouble(a, v) { return a.unshift(v); }
    noInline(unshiftDouble);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [1.5];
        shouldBe(unshiftDouble(array, 2.5), 2);
    }

    var array = [1.5];
    var r = unshiftDouble(array, NaN);
    shouldBe(r, 2);
    shouldBe(array.length, 2);
    shouldBe(Number.isNaN(array[0]), true);
    shouldBe(array[1], 1.5);
})();

// Multi-element unshift compiled under FTL: write all elements into the scratch
// buffer and call the multi-element operation.
(function () {
    function unshiftThree(a, x, y, z) { return a.unshift(x, y, z); }
    noInline(unshiftThree);
    for (var i = 0; i < testLoopCount; ++i) {
        var contig = ["x", "y"];
        shouldBe(unshiftThree(contig, "a", "b", "c"), 5);
        shouldBe(contig[0], "a");
        shouldBe(contig[2], "c");
        shouldBe(contig[3], "x");

        var ints = [10];
        shouldBe(unshiftThree(ints, 1, 2, 3), 4);
        shouldBe(ints[0], 1);
        shouldBe(ints[2], 3);
        shouldBe(ints[3], 10);

        var doubles = [10.5];
        shouldBe(unshiftThree(doubles, 1.5, 2.5, 3.5), 4);
        shouldBe(doubles[0], 1.5);
        shouldBe(doubles[2], 3.5);
        shouldBe(doubles[3], 10.5);
    }
})();

// Multi-element unshift NaN speculation under FTL.
(function () {
    function unshiftThree(a, x, y, z) { return a.unshift(x, y, z); }
    noInline(unshiftThree);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [10.5];
        shouldBe(unshiftThree(array, 1.5, 2.5, 3.5), 4);
    }

    var array = [10.5];
    var r = unshiftThree(array, 1.5, NaN, 3.5);
    shouldBe(r, 4);
    shouldBe(array.length, 4);
    shouldBe(array[0], 1.5);
    shouldBe(Number.isNaN(array[1]), true);
    shouldBe(array[2], 3.5);
    shouldBe(array[3], 10.5);
})();
