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

for (var i = 0; i < 10000; ++i)
    o.f = i;
o.f = 42;

for (var i = 0; i < 10000; ++i) {
    if (foo(o) !== 4200)
        throw new Error("bad result: " + result);
    var result = foo(f);
    if (!Number.isNaN(result))
        throw "Error: bad result: " + result;
}

var q = {};
q.f = 43;
var result = foo(q);
if (result != 100 * 43)
    throw "Error: bad result at end: " + result;
