function foo() {
    var result = 0;
    for (var i = 0; i < arguments.length; ++i)
        result += arguments[i];
    return result;
}

function bar() {
    return foo.apply(this, arguments);
}

function baz(p) {
    if (p)
        return bar(1, 42);
    return 0;
}

noInline(baz);

// Execute baz() once with p set, so that the call has a valid prediction.
baz(true);

// Warm up profiling in bar and foo. Convince this profiling that bar()'s varargs call will tend to
// pass a small number of arguments;
for (var i = 0; i < 1000; ++i)
    bar(1);

// Now compile baz(), but don't run the bad code yet.
for (var i = 0; i < 10000; ++i)
    baz(false);

// Finally, trigger the bug.
var result = baz(true);
if (result != 43)
    throw "Error: bad result: " + result;
