function foo(o) {
    return o.f;
}

noInline(foo);

var p = {};
var o = Object.create(p);

for (var i = 0; i < 10000; ++i) {
    var result = foo(o);
    if (result !== void 0)
        throw "Error: bad result in loop: " + result;
}

p.f = 42;
var result = foo(o);
if (result !== 42)
    throw "Error: bad result at end: " + result;
