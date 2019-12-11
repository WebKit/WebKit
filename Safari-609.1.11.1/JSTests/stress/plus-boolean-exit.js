function foo(a, b) {
    return a.f + b.f;
}

noInline(foo);

function test(a, b, c) {
    var result = foo({f:a}, {f:b});
    if (result != c)
        throw "Error: expected " + c + " but got: " + result;
}

for (var i = 0; i < 100000; ++i)
    test(true, 42, 43);

// Now try some unexpected things, in descending order of possible badness.
test(true, 2147483647, 2147483648);
test(false, 42, 42);
test(1, 2, 3);
test(true, true, 2);
test(1.5, 1.5, 3);

