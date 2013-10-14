function foo(o) {
    o.f = 42;
}

function bar(o) {
    o.f = 84;
}

for (var i = 0; i < 1000000; ++i) {
    var o = {};
    foo(o);
    if (o.f != 42)
        throw "Error: expected 42, got " + o.f;
    bar(o)
    if (o.f != 84)
        throw "Error: expected 84, got " + o.f;
}

