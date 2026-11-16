function shouldBe(expected, actual, msg = "") {
    if (msg)
        msg = " for " + msg;
    if (actual !== expected)
        throw new Error("bad value" + msg + ": " + actual + ". Expected " + expected);
}

function sink(p, q) {
    function* gen(x) { return x; }
    var g = gen(42);
    if (p) { if (q) OSRExit(); return g; }
    return { next() { return { value: 0, done: true }; } };
}
noInline(sink);

for (var i = 0; i < testLoopCount; ++i) {
    var g = sink(true, false);
    shouldBe(42, g.next().value);
}

// At this point, the function should be compiled down to the FTL

// Check that the generator object is properly allocated on OSR exit
var g = sink(true, true);
shouldBe(42, g.next().value);
