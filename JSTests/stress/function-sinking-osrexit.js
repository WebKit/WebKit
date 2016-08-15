function sink (p, q) {
    var g = function(x) { return x; };
    if (p) { if (q) OSRExit(); return g; }
    return function(x) { return x; };
}
noInline(sink);

for (var i = 0; i < 10000; ++i) {
    var f = sink(true, false);
    var result = f(42);
    if (result != 42)
    throw "Error: expected 42 but got " + result;
}

// At this point, the function should be compiled down to the FTL

// Check that the function is properly allocated on OSR exit
var f = sink(true, true);
var result = f(42);
if (result != 42)
    throw "Error: expected 42 but got " + result;
