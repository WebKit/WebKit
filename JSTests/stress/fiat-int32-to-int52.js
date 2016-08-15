function foo(o) {
    return fiatInt52(o.f) + 1;
}

noInline(foo);

var o = {f:42};

for (var i = 0; i < 1000000; ++i) {
    var result = foo(o);
    if (result != 43)
        throw "Error: bad result: " + result;
}
