function foo(a, b, c) {
    var o = {f:42};
    if (DFGTrue())
        o.f = a + b + c;
    return o.f | 0;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(2000000000, 2000000000, -2000000000);
    if (result != 2000000000 && result != 42)
        throw "Error: bad result: " + result;
}

