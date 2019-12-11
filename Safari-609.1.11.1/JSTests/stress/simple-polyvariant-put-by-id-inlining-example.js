function foo(o) {
    bar(o);
}

function fuzz(o) {
    bar(o);
}

function bar(o) {
    o.f = 42;
}

noInline(foo);
noInline(fuzz);

for (var i = 0; i < 100000; ++i) {
    var o = {};
    foo(o);
    if (o.f != 42)
        throw "Error: bad result: " + o.f;
    o = {f:23};
    var result = fuzz(o);
    if (o.f != 42)
        throw "Error: bad result: " + o.f;
}

