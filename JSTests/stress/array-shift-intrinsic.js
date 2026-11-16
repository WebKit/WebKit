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

// Multi-element Int32: drains via the element-move path (operationArrayShiftElements).
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

// Multi-element Double: drains via the element-move path.
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

// Multi-element Contiguous: drains via the element-move path.
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

// Length >= 2 with a hole at index 0: the element-move operation must bail to the
// generic path (it returns the empty value without mutating the array).
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [1, 2, 3];
        delete array[0];
        shouldBe(array.length, 3);
        shouldBe(shiftOnce(array), undefined);
        shouldBe(array.length, 2);
        shouldBe(array[0], 2);
        shouldBe(array[1], 3);
    }
})();

// Length >= 2 with a hole in the middle: the hole moves down like any other value.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [1, 2, 3];
        delete array[1];
        shouldBe(shiftOnce(array), 1);
        shouldBe(array.length, 2);
        shouldBe(0 in array, false);
        shouldBe(array[0], undefined);
        shouldBe(array[1], 3);
    }
})();

// Length == 128 (JSArray::shiftThreshold) takes the element-move path; length == 129
// takes the generic path. Both must produce the same result.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var a128 = [];
        for (var j = 0; j < 128; ++j)
            a128.push(j);
        shouldBe(shiftOnce(a128), 0);
        shouldBe(a128.length, 127);
        shouldBe(a128[0], 1);
        shouldBe(a128[126], 127);

        var a129 = [];
        for (var j = 0; j < 129; ++j)
            a129.push(j);
        shouldBe(shiftOnce(a129), 0);
        shouldBe(a129.length, 128);
        shouldBe(a129[0], 1);
        shouldBe(a129[127], 128);
    }
})();

// Same boundary with Contiguous (string) elements.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var a128 = [];
        for (var j = 0; j < 128; ++j)
            a128.push("v" + j);
        shouldBe(shiftOnce(a128), "v0");
        shouldBe(a128.length, 127);
        shouldBe(a128[0], "v1");
        shouldBe(a128[126], "v127");

        var a129 = [];
        for (var j = 0; j < 129; ++j)
            a129.push("v" + j);
        shouldBe(shiftOnce(a129), "v0");
        shouldBe(a129.length, 128);
        shouldBe(a129[0], "v1");
        shouldBe(a129[127], "v128");
    }
})();

// Arrays whose structure is no longer primordial still take the intrinsic: the
// element-move path is disabled for them, but the length 0 and length 1 paths move
// no elements and stay inlined, and every length must still shift correctly.
(function () {
    function shiftNonOriginal(array) {
        return array.shift();
    }
    noInline(shiftNonOriginal);

    for (var i = 0; i < testLoopCount; ++i) {
        var empty = [];
        empty.extra = 1;
        shouldBe(shiftNonOriginal(empty), undefined);
        shouldBe(empty.length, 0);

        var one = [7];
        one.extra = 1;
        shouldBe(shiftNonOriginal(one), 7);
        shouldBe(one.length, 0);

        var many = [1, 2, 3];
        many.extra = 1;
        shouldBe(shiftNonOriginal(many), 1);
        shouldBe(many.length, 2);
        shouldBe(many[0], 2);
        shouldBe(many[1], 3);
    }
})();

// Same, on an Array subclass, which also has a non-primordial structure.
(function () {
    class MyArray extends Array { }

    function shiftSubclass(array) {
        return array.shift();
    }
    noInline(shiftSubclass);

    for (var i = 0; i < testLoopCount; ++i) {
        var array = new MyArray();
        array.push(1, 2, 3);
        shouldBe(shiftSubclass(array), 1);
        shouldBe(array.length, 2);
        shouldBe(array[0], 2);
        shouldBe(array[1], 3);
        shouldBe(shiftSubclass(array), 2);
        shouldBe(shiftSubclass(array), 3);
        shouldBe(shiftSubclass(array), undefined);
        shouldBe(array.length, 0);
    }
})();

// Element-move boundaries around JSArray::shiftThreshold (128), which is the
// largest length the intrinsic moves elements for. Assert every slot, not just
// the returned value, for Int32, Double and Contiguous elements.
(function () {
    function checkShift(length, makeElement) {
        var array = [];
        for (var j = 0; j < length; ++j)
            array.push(makeElement(j));

        shouldBe(shiftOnce(array), makeElement(0));
        shouldBe(array.length, length - 1);
        for (var j = 0; j < length - 1; ++j)
            shouldBe(array[j], makeElement(j + 1));
        shouldBe(array[length - 1], undefined);
    }

    var int32Element = function (j) { return j + 1; };
    var doubleElement = function (j) { return j + 0.5; };
    var stringElement = function (j) { return "v" + j; };

    for (var i = 0; i < testLoopCount; ++i) {
        for (var length of [2, 3, 4, 5, 127, 128, 129]) {
            checkShift(length, int32Element);
            checkShift(length, doubleElement);
            checkShift(length, stringElement);
        }
    }
})();

// Double arrays cannot tell a hole from a stored NaN: both read back as PNaN, so
// the element move bails on either and lets the generic path decide. A hole must
// shift in undefined, a real NaN must shift out NaN, and both must leave the
// remaining elements correct. Values that are easy to corrupt in a raw double
// move (-0, the infinities) must survive with their bit patterns intact.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var hole = [1.5, 2.5, 3.5, 4.5];
        delete hole[0];
        shouldBe(shiftOnce(hole), undefined);
        shouldBe(hole.length, 3);
        shouldBe(hole[0], 2.5);
        shouldBe(hole[2], 4.5);

        var nan = [NaN, 2.5, 3.5, 4.5];
        var shifted = shiftOnce(nan);
        shouldBe(shifted !== shifted, true);
        shouldBe(nan.length, 3);
        shouldBe(nan[0], 2.5);
        shouldBe(nan[2], 4.5);

        var nanInMiddle = [1.5, NaN, 3.5, 4.5];
        shouldBe(shiftOnce(nanInMiddle), 1.5);
        shouldBe(nanInMiddle[0] !== nanInMiddle[0], true);
        shouldBe(nanInMiddle[1], 3.5);

        var signedZero = [1.5, -0, 2.5, 3.5];
        shouldBe(shiftOnce(signedZero), 1.5);
        shouldBe(1 / signedZero[0], -Infinity);

        var infinities = [1.5, Infinity, -Infinity, 2.5];
        shouldBe(shiftOnce(infinities), 1.5);
        shouldBe(infinities[0], Infinity);
        shouldBe(infinities[1], -Infinity);
    }
})();

// A hole at slot 0 must leave the array unmutated before the generic path takes
// over: the element move reports the hole without touching the elements.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        for (var length of [2, 3, 5, 128]) {
            var array = [];
            for (var j = 0; j < length; ++j)
                array.push(j + 1);
            delete array[0];

            shouldBe(shiftOnce(array), undefined);
            shouldBe(array.length, length - 1);
            for (var j = 0; j < length - 1; ++j)
                shouldBe(array[j], j + 2);
        }
    }
})();

// Draining walks one array down through every length, so a single call site sees
// the element move, then the length 1 case, then the length 0 case.
(function () {
    for (var i = 0; i < testLoopCount; ++i) {
        var array = [];
        for (var j = 0; j < 20; ++j)
            array.push("d" + j);

        for (var j = 0; j < 20; ++j) {
            shouldBe(array.length, 20 - j);
            shouldBe(shiftOnce(array), "d" + j);
        }
        shouldBe(array.length, 0);
        shouldBe(shiftOnce(array), undefined);
    }
})();

// Shifting a Contiguous array moves cell references down into slots the
// concurrent marker may already have scanned, which is why the element move ends
// with a write barrier on the array. Shift cells while allocating and collecting,
// keeping the moved-down cells reachable only through the array.
(function () {
    function shiftCells(array) {
        return array.shift();
    }
    noInline(shiftCells);

    for (var i = 0; i < 100; ++i) {
        var array = [];
        for (var j = 0; j < 64; ++j)
            array.push({ index: j, payload: "p" + j });

        // Promote the array out of eden so it can be scanned before we mutate it.
        gc();

        for (var j = 0; j < 32; ++j) {
            var shifted = shiftCells(array);
            shouldBe(shifted.index, j);

            // Allocate to drive the collector forward while the array holds the
            // moved-down cells.
            for (var k = 0; k < 100; ++k)
                new Object();
            if (!(j % 8))
                edenGC();

            // Every surviving cell must still be intact and correctly ordered.
            shouldBe(array.length, 63 - j);
            shouldBe(array[0].index, j + 1);
            shouldBe(array[0].payload, "p" + (j + 1));
            shouldBe(array[array.length - 1].index, 63);
        }
    }
})();

// Everything below pollutes Array.prototype with an indexed property, which
// permanently invalidates the arrayPrototypeChainIsSane watchpoint for the rest of
// this VM: deleting the property again does not re-arm it. Any test that needs the
// intrinsic's element-move path must run before this point.

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

// Polluting Array.prototype with an indexed property invalidates the
// arrayPrototypeChainIsSane watchpoint. Compiled code must fall back to the
// generic path, where a hole reads through the prototype chain.
(function () {
    function shiftIsolated(array) {
        return array.shift();
    }
    noInline(shiftIsolated);

    for (var i = 0; i < testLoopCount; ++i)
        shiftIsolated([i, i + 1, i + 2]);

    Array.prototype[1] = "proto-one";
    try {
        var array = [10, 20, 30];
        delete array[1];
        shouldBe(array.length, 3);
        shouldBe(shiftIsolated(array), 10);
        shouldBe(array.length, 2);
        shouldBe(array[0], "proto-one");
        shouldBe(array[1], 30);
    } finally {
        delete Array.prototype[1];
    }
})();
