function foo(o) {
    var result = 0;
    for (var i = 0; i < 100; ++i)
        result += o.f;
    return result;
}

noInline(foo);

var p = {f:42};
var o = Object.create(p);

for (var i = 0; i < 10000; ++i) {
    p["i" + i] = i;
    var result = foo(o);
    if (result != 42 * 100)
        throw "Error: bad result: " + result;
}
