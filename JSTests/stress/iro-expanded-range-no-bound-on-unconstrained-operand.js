// Regression test for an IRO miscompile in expandedRangeFor.

function foo(x) {
    return ((x + 1) | 0) + (x + 1);
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    var result = foo(i);
    if (result != (i + 1) * 2)
        throw "Error: bad result for i = " + i + ": " + result;
}

var result = foo(2147483647);
if (result != 0)
    throw "Error: bad result for INT32_MAX: " + result + " (expected 0)";
