function foo(o) {
    o.f = 42;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var o = Object.create(null);
    foo(o);
    if (o.f != 42)
        throw "Error: bad result: " + o.f;
}

