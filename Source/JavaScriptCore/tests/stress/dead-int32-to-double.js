function foo(int, o) {
    var x = int;
    o.f = x;
    for (var i = 0; i < 100; ++i)
        x += 0.5;
}

noInline(foo);

for (var i = 0; i < 100; ++i)
    foo(42, {});

var o = {g: 43};
foo(47, o);
if (o.f != 47)
    throw "Error: o.f is " + o.f;
