function sink (p, q) {
    var g = x => x;
    if (p) { if (q) g.inner = 42; return g; }
    return x => x;
}
noInline(sink);

for (var i = 0; i < 10000; ++i) {
    var f = sink(true, true);
    var result = f(42);
    if (result != 42)
    throw "Error: expected 42 but got " + result;
}

// At this point, the function should be compiled down to the FTL

// Test the allocation on the implicit inner else branch
var f = sink(true, false);
var result = f(12);
if (result != 12)
    // This shouldn't matter as it should be either correct or completely crash
    throw "Error: expected 12 but got " + result;

// Check that the allocation did not sink beyond the property assignment
var f = sink(true, true);
var result = f.inner;
if (result != 42)
    throw "Error: inner should be 42 but is " + result;
