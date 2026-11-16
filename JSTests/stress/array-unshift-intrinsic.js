function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Bad value: " + actual + ", expected: " + expected);
}

function unshiftOnce(array, v) {
    return array.unshift(v);
}
noInline(unshiftOnce);

function unshiftMany(array, a, b, c) {
    return array.unshift(a, b, c);
}
noInline(unshiftMany);

// Length-0 Contiguous fast path.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = ["seed"];
        array.length = 0;
        shouldBe(unshiftOnce(array, "x"), 1);
        shouldBe(array.length, 1);
        shouldBe(array[0], "x");
    }
})();

// Length-1 Contiguous fast path: existing element shifts to [1].
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = ["existing"];
        shouldBe(unshiftOnce(array, "front"), 2);
        shouldBe(array.length, 2);
        shouldBe(array[0], "front");
        shouldBe(array[1], "existing");
    }
})();

// Length-0 Int32 fast path.
(function () {
    function unshiftInt(a, v) { return a.unshift(v); }
    noInline(unshiftInt);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [42];
        array.length = 0;
        shouldBe(unshiftInt(array, 7), 1);
        shouldBe(array.length, 1);
        shouldBe(array[0], 7);
    }
})();

// Length-1 Int32 fast path.
(function () {
    function unshiftInt(a, v) { return a.unshift(v); }
    noInline(unshiftInt);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [99];
        shouldBe(unshiftInt(array, 7), 2);
        shouldBe(array.length, 2);
        shouldBe(array[0], 7);
        shouldBe(array[1], 99);
    }
})();

// Length-0 Double fast path.
(function () {
    function unshiftDouble(a, v) { return a.unshift(v); }
    noInline(unshiftDouble);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [1.5];
        array.length = 0;
        shouldBe(unshiftDouble(array, 2.5), 1);
        shouldBe(array.length, 1);
        shouldBe(array[0], 2.5);
    }
})();

// Length-1 Double fast path.
(function () {
    function unshiftDouble(a, v) { return a.unshift(v); }
    noInline(unshiftDouble);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [1.5];
        shouldBe(unshiftDouble(array, 2.5), 2);
        shouldBe(array.length, 2);
        shouldBe(array[0], 2.5);
        shouldBe(array[1], 1.5);
    }
})();

// Length-1 Double hole (NaN-bit-pattern at [0]): fast path branchIfNaN takes slow case.
(function () {
    function unshiftDouble(a, v) { return a.unshift(v); }
    noInline(unshiftDouble);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [1.5];
        delete array[0];
        shouldBe(array.length, 1);
        shouldBe(unshiftDouble(array, 2.5), 2);
        shouldBe(array.length, 2);
        shouldBe(array[0], 2.5);
    }
})();

// NaN value passed after warmup: DoubleRepRealUse speculation must OSR exit, and the
// slow path must produce a valid 2-element array. Run enough iterations to warm up
// the JIT, then pass NaN once.
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

// NaN value for a length-0 Double array after warmup.
(function () {
    function unshiftDouble(a, v) { return a.unshift(v); }
    noInline(unshiftDouble);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [2.5];
        array.length = 0;
        shouldBe(unshiftDouble(array, 3.5), 1);
    }

    var array = [2.5];
    array.length = 0;
    var r = unshiftDouble(array, NaN);
    shouldBe(r, 1);
    shouldBe(array.length, 1);
    shouldBe(Number.isNaN(array[0]), true);
})();

// Length-0 Double then unshift: result must be spec-correct regardless of
// whether the inline fast path or the slow path is taken.
(function () {
    function unshiftDouble(a, v) { return a.unshift(v); }
    noInline(unshiftDouble);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [1.5];
        array.length = 0;
        var seenLength = unshiftDouble(array, 3.5);
        shouldBe(seenLength, 1);
        shouldBe(array.length, 1);
        shouldBe(array[0], 3.5);
    }
})();

// Length >= 2: slow path via operationArrayUnshift.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = ["a", "b", "c"];
        shouldBe(unshiftOnce(array, "z"), 4);
        shouldBe(array.length, 4);
        shouldBe(array[0], "z");
        shouldBe(array[1], "a");
        shouldBe(array[2], "b");
        shouldBe(array[3], "c");
    }
})();

// Multi-arg unshift (3 args): the intrinsic now handles this via the multi-element
// scratch-buffer path. Verify the result is spec-correct.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = ["a"];
        shouldBe(unshiftMany(array, 1, 2, 3), 4);
        shouldBe(array.length, 4);
        shouldBe(array[0], 1);
        shouldBe(array[1], 2);
        shouldBe(array[2], 3);
        shouldBe(array[3], "a");
    }
})();

// Two-arg unshift on Int32 array.
(function () {
    function unshiftTwo(a, x, y) { return a.unshift(x, y); }
    noInline(unshiftTwo);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [10];
        shouldBe(unshiftTwo(array, 1, 2), 3);
        shouldBe(array.length, 3);
        shouldBe(array[0], 1);
        shouldBe(array[1], 2);
        shouldBe(array[2], 10);
    }
})();

// Two-arg unshift on Double array (exercises operationArrayUnshiftDoubleMultiple).
(function () {
    function unshiftTwo(a, x, y) { return a.unshift(x, y); }
    noInline(unshiftTwo);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [10.5];
        shouldBe(unshiftTwo(array, 1.5, 2.5), 3);
        shouldBe(array.length, 3);
        shouldBe(array[0], 1.5);
        shouldBe(array[1], 2.5);
        shouldBe(array[2], 10.5);
    }
})();

// Multi-arg unshift on a length-0 array (no memmove needed; pure write at front).
(function () {
    function unshiftThree(a, x, y, z) { return a.unshift(x, y, z); }
    noInline(unshiftThree);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [];
        shouldBe(unshiftThree(array, 1, 2, 3), 3);
        shouldBe(array.length, 3);
        shouldBe(array[0], 1);
        shouldBe(array[1], 2);
        shouldBe(array[2], 3);
    }
})();

// Multi-arg unshift on a Contiguous array with multiple existing elements
// (exercises memmove of elementCount slots).
(function () {
    function unshiftFour(a, p, q, r, s) { return a.unshift(p, q, r, s); }
    noInline(unshiftFour);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = ["x", "y", "z"];
        shouldBe(unshiftFour(array, "a", "b", "c", "d"), 7);
        shouldBe(array.length, 7);
        shouldBe(array[0], "a");
        shouldBe(array[1], "b");
        shouldBe(array[2], "c");
        shouldBe(array[3], "d");
        shouldBe(array[4], "x");
        shouldBe(array[5], "y");
        shouldBe(array[6], "z");
    }
})();

// Multi-arg unshift NaN speculation: passing a NaN in any slot must OSR exit
// (DoubleRepRealUse on each element). After warmup, feed a NaN and verify the
// slow path still produces a valid array.
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

// Multi-arg unshift on Int32 array with non-int (should refine arrayMode to
// Double or Contiguous depending on prediction).
(function () {
    function unshiftTwo(a, x, y) { return a.unshift(x, y); }
    noInline(unshiftTwo);
    // Warm up with all int values.
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [3];
        shouldBe(unshiftTwo(array, 1, 2), 3);
    }
    // Then pass an object: array gets promoted (or fast path bails) and the
    // operation slow path handles it.
    var array = [3];
    var o = { toString() { return "obj"; } };
    var r = unshiftTwo(array, "a", o);
    shouldBe(r, 3);
    shouldBe(array[0], "a");
    shouldBe(array[1], o);
    shouldBe(array[2], 3);
})();

// Many-element unshift (exercise scratch buffer with an interesting size).
(function () {
    function unshift10(a, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) {
        return a.unshift(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
    }
    noInline(unshift10);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [100];
        shouldBe(unshift10(array, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9), 11);
        shouldBe(array.length, 11);
        for (var k = 0; k < 10; ++k)
            shouldBe(array[k], k);
        shouldBe(array[10], 100);
    }
})();

// Zero-arg unshift: the intrinsic inlines this as a length read.
(function () {
    function unshiftNone(a) { return a.unshift(); }
    noInline(unshiftNone);
    for (var i = 0; i < testLoopCount; ++i) {
        var array = ["a", "b", "c"];
        shouldBe(unshiftNone(array), 3);
        shouldBe(array.length, 3);
        shouldBe(array[0], "a");
        shouldBe(array[2], "c");

        var empty = [];
        shouldBe(unshiftNone(empty), 0);
        shouldBe(empty.length, 0);
    }
})();

// Length-1 hole: storage[0] is empty, fast path must take slow case.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = ["filler"];
        delete array[0];
        shouldBe(array.length, 1);
        shouldBe(unshiftOnce(array, "new-front"), 2);
        shouldBe(array.length, 2);
        shouldBe(array[0], "new-front");
        shouldBe(array[1], undefined);
    }
})();

// Repeated unshifts grow the array beyond the inline length-1 path.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [];
        unshiftOnce(array, "a");
        unshiftOnce(array, "b");
        unshiftOnce(array, "c");
        unshiftOnce(array, "d");
        shouldBe(array.length, 4);
        shouldBe(array[0], "d");
        shouldBe(array[1], "c");
        shouldBe(array[2], "b");
        shouldBe(array[3], "a");
    }
})();

// ArrayStorage indexing: the intrinsic bails to the native function and the
// result must still match spec. We force ArrayStorage by writing a sparse index.
(function () {
    function unshiftStorage(a, v) { return a.unshift(v); }
    noInline(unshiftStorage);
    var template = [];
    template[100000] = "sparse";
    template.length = 3;
    for (var i = 0; i < testLoopCount; ++i) {
        var array = template.slice();
        // Force ArrayStorage on this instance too.
        array[100000] = "sparse";
        array.length = 3;
        shouldBe(unshiftStorage(array, "front"), 4);
        shouldBe(array.length, 4);
        shouldBe(array[0], "front");
    }
})();

// Length-1 with Array.prototype[1] setter installed BEFORE warmup: the intrinsic
// can never inline (prototype chain isn't sane at compile time), and the slow
// path is spec-compliant: it does [[Set]] which walks the prototype chain and
// invokes the setter. The setter doesn't store, so array[1] is undefined.
(function () {
    function unshiftIsolated(array, v) {
        return array.unshift(v);
    }
    noInline(unshiftIsolated);

    var setterCalls = 0;
    Array.prototype.__defineSetter__("1", function() { setterCalls++; });
    try {
        for (var i = 0; i < testLoopCount; ++i) {
            var array = ["only"];
            unshiftIsolated(array, "front");
            shouldBe(array.length, 2);
            shouldBe(array[0], "front");
            shouldBe(array[1], undefined);
        }
        shouldBe(setterCalls, testLoopCount);
    } finally {
        delete Array.prototype[1];
    }
})();

// Same scenario but install the setter AFTER warmup. The inline fast path is
// JIT'd against a sane prototype chain. Installing the setter must invalidate
// the inline code; the subsequent call goes through the slow path which calls
// the setter (matching the pre-warmup case).
(function () {
    function unshiftIsolated(array, v) {
        return array.unshift(v);
    }
    noInline(unshiftIsolated);

    for (var i = 0; i < testLoopCount; ++i)
        unshiftIsolated(["a"], "x");

    var setterCalls = 0;
    Array.prototype.__defineSetter__("1", function() { setterCalls++; });
    try {
        var array = ["only"];
        unshiftIsolated(array, "front");
        shouldBe(array.length, 2);
        shouldBe(array[0], "front");
        shouldBe(array[1], undefined);
        shouldBe(setterCalls, 1);
    } finally {
        delete Array.prototype[1];
    }
})();
