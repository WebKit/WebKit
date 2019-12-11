function foo(o, a, b, c, d, e, f, g, h, i, j) {
    return o.f;
}

function bar(o) {
    return foo(o);
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 100000; ++i)
    bar({f:42});

var result = bar({g:24, f:43});
if (result != 43)
    throw "Error: bad result: " + result;

