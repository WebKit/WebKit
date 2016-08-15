function bar() {
    return arguments;
}

function foo(p) {
    var a = bar(1, 2, 3);
    var b;
    if (p)
        b = bar(4, 5, 6);
    else
        b = [7, 8, 9];
    return (a[0] << 0) + (a[1] << 1) + (a[2] << 2) + (b[0] << 3) + (b[1] << 4) + (b[2] << 5);
}

noInline(foo);

for (var i = 0; i < 20000; ++i) {
    var p = i & 1;
    var q = (!p) * 3;
    var result = foo(p);
    if (result != (1 << 0) + (2 << 1) + (3 << 2) + ((4 + q) << 3) + ((5 + q) << 4) + ((6 + q) << 5))
        throw "Error: bad result: " + result;
}

