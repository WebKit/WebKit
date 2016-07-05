// Require lots of arguments so that arity fixup will need a lot of stack, making
// it prone to stack overflow.
var script = "";
for (var i = 0; i < 128; ++i)
    script += "dummy, "
script += "dummy";
var g = new Function(script, "return arguments;"); // Ensure that arguments are observed.

function f(recursionCount)
{
    if (!recursionCount)
        return;

    // Use too few arguments to force arity fixup.
    g();

    f(--recursionCount);
}

noInline(g);
noInline(f);

// Ensure that f and g get optimized.
for (var i = 0; i < 1000000; ++i) {
    // Recurse once to ensure profiling along all control flow paths.
    f(1);
}

try {
    // Recurse enough times to trigger a stack overflow exception.
    f(1000000);
} catch(e) {
    if (! (e instanceof RangeError))
        throw "bad value for e";
}
