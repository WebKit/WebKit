function foo(o) {
    for (var i = 0; i < 100; ++i)
        o.f = (o.f | 0) + 42;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var object;
    if ((i % 3) == 0)
        object = {g:3};
    else if ((i % 3) == 1)
        object = {f:1, g:2};
    else if ((i % 3) == 2)
        object = {g:1, f:2};
    foo(object);
    if (object.f != 42 * 100 + (i % 3))
        throw "Error: bad result for i = " + i + ": " + object.f;
}

var r = {g:3, h:4, f:5};
foo(r);
if (r.f != 5 + 42 * 100)
    throw "Error: bad result at end: " + r.f;
