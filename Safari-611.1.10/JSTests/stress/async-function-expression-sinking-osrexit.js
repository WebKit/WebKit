function shouldBe(expected, actual, msg = "") {
    if (msg)
        msg = " for " + msg;
    if (actual !== expected)
        throw new Error("bad value" + msg + ": " + actual + ". Expected " + expected);
}
function shouldBeAsync(expected, promise, msg) {
    let actual;
    var hadError = false;
    promise.then(function(value) { actual = value; },
                 function(error) { hadError = true; actual = error; });
    drainMicrotasks();

    if (hadError)
        throw actual;

    shouldBe(expected, actual, msg);
}

var AsyncFunctionPrototype = async function(){}.__proto__;

function sink (p, q) {
    var g = async function (x) { return x; };
    if (p) { if (q) OSRExit(); return g; }
    return async function (x) { return x; };
}
noInline(sink);

for (var i = 0; i < 10000; ++i) {
    var f = sink(true, false);
    shouldBe(f.__proto__, AsyncFunctionPrototype);
    shouldBeAsync(42, f(42));
}

// At this point, the function should be compiled down to the FTL

// Check that the function is properly allocated on OSR exit
var f = sink(true, true);
shouldBe(f.__proto__, AsyncFunctionPrototype);
shouldBeAsync(42, f(42));
