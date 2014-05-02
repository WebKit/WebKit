function foo(a, b) {
    var o = {f:42};
    if (DFGTrue())
        o.f = -(a + b);
    return o.f | 0;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(1073741824, 1073741824);
    if (result != -2147483648 && result != 42)
        throw "Error: bad result: " + result;
}

