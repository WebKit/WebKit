function foo(a, b) {
    return Math.max(a.f, b.f);
}

noInline(foo);

var f = new Float64Array(1);
var i = new Int32Array(f.buffer);

function test(a, b, c) {
    var result = foo({f:a}, {f:b});
    f[0] = c;
    var expectedA = i[0];
    var expectedB = i[1];
    f[0] = result;
    if (i[0] != expectedA || i[1] != expectedB)
        throw "Error: expected " + c + " but got: " + result;
}

for (var i = 0; i < 100000; ++i)
    test(true, 42, 42);

// Now try some unexpected things, in descending order of possible badness.
test(true, 2147483647, 2147483647);
test(false, 42, 42);
test(false, -42, -0);
test(1, 2, 2);
test(true, true, 1);
test(1.5, 1.5, 2.25);

