function foo(a, o) {
    var x = a >>> 0;
    o.f = x | 0;
    for (var i = 0; i < 100; ++i)
        x++;
}

noInline(foo);

for (var i = 0; i < 100; ++i)
    foo(42, {});

var o = {g: 43};
foo(47, o);
if (o.f != 47)
    throw "Error: o.f is " + o.f;
