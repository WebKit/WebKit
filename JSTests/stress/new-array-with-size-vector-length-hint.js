function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

// Array.prototype.filter result must have the exact length of the number of
// elements that passed the predicate, regardless of the vectorLengthHint
// learned by ArrayAllocationProfile in earlier tiers.
(function() {
    function test(array) {
        return array.filter(x => x % 3 === 0);
    }
    noInline(test);

    var array = [];
    for (var i = 0; i < 16; ++i)
        array.push(i);

    for (var iter = 0; iter < 1e5; ++iter) {
        var r = test(array);
        shouldBe(r.length, 6);
        shouldBe(r[0], 0);
        shouldBe(r[5], 15);
        shouldBe(6 in r, false);
    }
})();

// No elements pass; result must be empty even if a non-zero vectorLengthHint
// was learned for the allocation site.
(function() {
    function test(array) {
        return array.filter(x => false);
    }
    noInline(test);

    var array = [1, 2, 3, 4, 5, 6, 7, 8];
    for (var iter = 0; iter < 1e5; ++iter) {
        var r = test(array);
        shouldBe(r.length, 0);
        shouldBe(0 in r, false);
    }
})();

// new Array(n) with subsequent growth: vectorLengthHint must not affect the
// observable publicLength.
(function() {
    function test(n) {
        var a = new Array(n);
        for (var i = 0; i < 20; ++i)
            a[i] = i;
        return a;
    }
    noInline(test);

    for (var iter = 0; iter < 1e5; ++iter) {
        var r = test(3);
        shouldBe(r.length, 20);
        shouldBe(r[0], 0);
        shouldBe(r[19], 19);
    }
})();

// @newArrayWithSize via Array.from on array-like: ensure correctness with
// the learned hint.
(function() {
    function test(obj) {
        return Array.from(obj);
    }
    noInline(test);

    var obj = { length: 8, 0: 0, 1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 6: 6, 7: 7 };
    for (var iter = 0; iter < 1e5; ++iter) {
        var r = test(obj);
        shouldBe(r.length, 8);
        shouldBe(r[7], 7);
        shouldBe(8 in r, false);
    }
})();
