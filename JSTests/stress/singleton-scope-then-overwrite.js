function foo(a) {
    var x = a + 1;
    var f = function(a) {
        return x + a;
    };
    noInline(f);
    for (var i = 0; i < 10000; ++i) {
        var result = f(i);
        if (result != a + 1 + i)
            throw "Error: bad result: " + result;
    }
    x = 999;
    var result = f(1);
    if (result != 999 + 1)
        throw "Error: bad result: " + result;
}

noInline(foo);
for (var i = 0; i < 3; ++i)
    foo(42 + i);
