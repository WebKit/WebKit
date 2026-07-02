function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Bad value: " + actual + ", expected: " + expected);
}

function shiftOnce(array) {
    return array.shift();
}
noInline(shiftOnce);

function drain(array) {
    var results = [];
    while (array.length)
        results.push(array.shift());
    return results;
}
noInline(drain);

// Length-0: fast path returns undefined.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [];
        shouldBe(shiftOnce(array), undefined);
        shouldBe(array.length, 0);
    }
})();

// Length-1 Int32 fast path.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [42];
        shouldBe(shiftOnce(array), 42);
        shouldBe(array.length, 0);
    }
})();

// Length-1 Double fast path.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [3.5];
        shouldBe(shiftOnce(array), 3.5);
        shouldBe(array.length, 0);
    }
})();

// Length-1 Contiguous fast path.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = ["only"];
        shouldBe(shiftOnce(array), "only");
        shouldBe(array.length, 0);
    }
})();

// Multi-element Int32: drains via slow path (operationArrayShift).
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var results = drain([1, 2, 3, 4, 5]);
        shouldBe(results.length, 5);
        shouldBe(results[0], 1);
        shouldBe(results[1], 2);
        shouldBe(results[2], 3);
        shouldBe(results[3], 4);
        shouldBe(results[4], 5);
    }
})();

// Multi-element Double: drains via slow path.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var results = drain([1.5, 2.5, 3.5, 4.5]);
        shouldBe(results.length, 4);
        shouldBe(results[0], 1.5);
        shouldBe(results[1], 2.5);
        shouldBe(results[2], 3.5);
        shouldBe(results[3], 4.5);
    }
})();

// Multi-element Contiguous: drains via slow path.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var results = drain(["a", "b", "c"]);
        shouldBe(results.length, 3);
        shouldBe(results[0], "a");
        shouldBe(results[1], "b");
        shouldBe(results[2], "c");
    }
})();

// Length-1 Int32 hole: storage[0] is empty, fast path must take slow case.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [1];
        delete array[0];
        shouldBe(array.length, 1);
        shouldBe(shiftOnce(array), undefined);
        shouldBe(array.length, 0);
    }
})();

// Length-1 Double hole: storage[0] is NaN-bit-pattern, fast path must take slow case.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [1.5];
        delete array[0];
        shouldBe(array.length, 1);
        shouldBe(shiftOnce(array), undefined);
        shouldBe(array.length, 0);
    }
})();

// Length-1 Double containing real NaN: fast path branchIfNaN takes slow case but result must be NaN.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [NaN];
        var result = shiftOnce(array);
        shouldBe(result !== result, true);
        shouldBe(array.length, 0);
    }
})();

// Repeated shift on the same array exercises arrayMode transitions and slow path.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [10, 20, 30];
        shouldBe(shiftOnce(array), 10);
        shouldBe(shiftOnce(array), 20);
        shouldBe(shiftOnce(array), 30);
        shouldBe(shiftOnce(array), undefined);
        shouldBe(array.length, 0);
    }
})();

// With Array.prototype[0] set, a length-1 hole array must shift to the
// prototype value. The intrinsic's slow path (operationArrayShift) reads
// through the prototype chain via getIndex.
(function () {
    function shiftIsolated(array) {
        return array.shift();
    }
    noInline(shiftIsolated);

    for (var i = 0; i < testLoopCount; ++i)
        shiftIsolated([1]);

    Array.prototype[0] = "proto-zero";
    try {
        var array = [42];
        delete array[0];
        shouldBe(shiftIsolated(array), "proto-zero");
        shouldBe(array.length, 0);
    } finally {
        delete Array.prototype[0];
    }
})();
