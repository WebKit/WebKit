function foo(a, b, p, o) {
    var c = a + b;
    if (p)
        c -= o.f;
    return c + 1;
}

noInline(foo);

var o = {f: 42};
for (var i = 0; i < 100000; ++i) {
    var result = foo(2000000000, 2000000000, false, o);
    if (result != 4000000001)
        throw "Error: bad result: " + result;
}
