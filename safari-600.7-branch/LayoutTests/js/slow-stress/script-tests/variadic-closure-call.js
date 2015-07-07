function foo(a) {
    var x = 42 + a;
    function bar(a, b, c) {
        var result = a + b + c;
        for (var i = 3; i < arguments.length; ++i)
            result = arguments[i] * x;
        return result;
    }
    return bar.apply(this, arguments);
}

// Warm everything up with variadic calls that don't fail arity checks.
for (var i = 0; i < 100000; ++i) {
    var args = [];
    var n = (i % 8) + 8;
    for (var j = 0; j < n; ++j)
        args.push(j);
    var result = foo.apply(null, args);
    if (("" + result) != ("" + [294, 336, 378, 420, 462, 504, 546, 588][n - 8]))
        throw "Error: bad result for i = " + i + ": " + result;
}

// Start failing some arity checks.
for (var i = 0; i < 100000; ++i) {
    var args = [];
    var n = i % 16;
    for (var j = 0; j < n; ++j)
        args.push(j);
    var result = foo.apply(null, args);
    if (("" + result) != ("" + [0/0, 0/0, 0/0, 3, 126, 168, 210, 252, 294, 336, 378, 420, 462, 504, 546, 588][n]))
        throw "Error: bad result for i = " + i + ": " + result;
}

