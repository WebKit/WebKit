function foo(a, b) {
    return Math.max(a.f, b.f);
}

noInline(foo);

function test(a, b, c) {
    var result = foo({f:a}, {f:b});
    if (result != c)
        throw "Error: expected " + c + " but got: " + result;
}

for (var i = 0; i < 100000; ++i)
    test(true, 42, 42);

// Now try some unexpected things, in descending order of possible badness.
test(true, 2147483647, 2147483647);
test(false, 42, 42);
test(1, 2, 2);
test(true, true, 1);
test(1.5, 1.5, 1.5);

