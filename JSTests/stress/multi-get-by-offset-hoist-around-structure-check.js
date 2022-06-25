function foo(o, start) {
    var result = 0;
    for (var i = 0; i < 100; ++i)
        result += o.f;
    return result;
}

noInline(foo);

var o = {};
o.f = 42;
var f = {};
f.f = 43;
f.g = 44;

for (var i = 0; i < 10000; ++i)
    o.f = i;
o.f = 42;

for (var i = 0; i < 10000; ++i) {
    var p;
    if (i & 1)
        p = o;
    else
        p = Object.create(o);
    var result = foo(p);
    if (result != 100 * 42)
        throw "Error: bad result: " + result;
}

