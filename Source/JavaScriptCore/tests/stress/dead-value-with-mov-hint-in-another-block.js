function foo(a, b, p, o) {
    var x = a + b;
    if (p) {
        var y = x;
        var result = o.f.f;
        var z = y + 1;
        return result;
    }
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(1, 2, true, {f:{f:42}});
    if (result != 42)
        throw "Error: bad result: " + result;
}
