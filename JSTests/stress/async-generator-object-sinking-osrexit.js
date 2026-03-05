function shouldBe(expected, actual, msg = "") {
    if (msg)
        msg = " for " + msg;
    if (actual !== expected)
        throw new Error("bad value" + msg + ": " + actual + ". Expected " + expected);
}

function shouldBeAsyncValue(expected, promise, msg) {
    let actual;
    var hadError = false;
    promise.then(function(value) { actual = value; },
                 function(error) { hadError = true; actual = error; });
    drainMicrotasks();

    if (hadError)
        throw actual;

    shouldBe(expected, actual, msg);
}

function sink(p, q) {
    async function* gen(x) { return x; }
    var g = gen(42);
    if (p) { if (q) OSRExit(); return g; }
    return {};
}
noInline(sink);

for (var i = 0; i < testLoopCount; ++i) {
    var g = sink(true, false);
    shouldBeAsyncValue(42, g.next().then(r => r.value));
}

// At this point, the function should be compiled down to the FTL

// Check that the async generator object is properly allocated on OSR exit
var g = sink(true, true);
shouldBeAsyncValue(42, g.next().then(r => r.value));
