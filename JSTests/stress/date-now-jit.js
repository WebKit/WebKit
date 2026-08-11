function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// Date.now() must return a finite, non-negative integer through all JIT tiers.
function basic() {
    return Date.now();
}
noInline(basic);

for (var i = 0; i < testLoopCount; ++i) {
    var t = basic();
    shouldBe(typeof t, 'number');
    shouldBe(Number.isFinite(t), true);
    shouldBe(Number.isInteger(t), true);
    shouldBe(t >= 0, true);
}

// Two Date.now() calls inside the same hot function must not be CSE'd into
// one, and the call inside the loop must not be LICM-hoisted out. If either
// happened, the loop below would never observe the clock advancing and the
// spin guard would fire.
function spinUntilAdvance() {
    var start = Date.now();
    var spin = 0;
    var t;
    do {
        t = Date.now();
        if (++spin > 1e8)
            throw new Error('Date.now() did not advance after ' + spin + ' iterations; likely CSE/LICM bug');
    } while (t === start);
    return t - start;
}
noInline(spinUntilAdvance);

for (var i = 0; i < 200; ++i) {
    var diff = spinUntilAdvance();
    if (diff <= 0)
        throw new Error('Date.now() went backwards: diff=' + diff);
}
