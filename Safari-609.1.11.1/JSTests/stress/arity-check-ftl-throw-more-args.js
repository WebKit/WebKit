// Require lots of arguments so that arity fixup will need a lot of stack, making
// it prone to stack overflow.
var script = "recursionCount, ";
for (var i = 0; i < 5000; ++i)
    script += "dummy, "
script += "dummy";
var g = new Function(script, "return recursionCount ? g(recursionCount - 1) : 0;"); // Ensure that arguments are observed.

noInline(g);

// Ensure that f and g get optimized.
for (var i = 0; i < 10000; ++i) {
    // Recurse once to ensure profiling along all control flow paths.
    g(1);
}

try {
    // Recurse enough times to trigger a stack overflow exception.
    g(1000000);
} catch(e) {
    if (! (e instanceof RangeError))
        throw "bad value for e";
}
