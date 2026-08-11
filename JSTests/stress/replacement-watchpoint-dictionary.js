function foo(o) {
    return o.f;
}

noInline(foo);

var p = {f:42};
var o = Object.create(p);

for (var i = 0; i < 100; ++i) {
    p["i" + i] = i;
    for (var j = 0; j < 100; ++j) {
        var result = foo(o);
        if (result != 42)
            throw "Error: bad result: " + result;
    }
}

// Make p a non-dictionary.
for (var i = 0; i < 100; ++i) {
    var tmp = o.f;
}

p.f = 43;
var result = foo(o);
if (result != 43)
    throw "Error: bad result at end: " + result;
