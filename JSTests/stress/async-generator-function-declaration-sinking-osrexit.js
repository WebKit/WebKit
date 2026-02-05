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

var AsyncGeneratorFunctionPrototype = (async function*(){}).__proto__;

function sink (p, q) {
    async function *g(x) { yield x; };
    if (p) { if (q) OSRExit(); return g; }
    async function *f(x) { yield x; };
    return f;
}
noInline(sink);

for (var i = 0; i < testLoopCount; ++i) {
    var f = sink(true, false);
    shouldBe(AsyncGeneratorFunctionPrototype, f.__proto__);
    shouldBeAsyncValue(42, f(42).next().then(r => r.value));
}

// At this point, the function should be compiled down to the FTL

// Check that the function is properly allocated on OSR exit
var f = sink(true, true);
shouldBe(AsyncGeneratorFunctionPrototype, f.__proto__);
shouldBeAsyncValue(42, f(42).next().then(r => r.value));
