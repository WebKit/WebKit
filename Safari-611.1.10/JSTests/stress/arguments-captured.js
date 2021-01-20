function foo(o) {
    o[0] = 42;
}

function bar(a) {
    var o = {};
    o.f = a;
    foo(arguments);
    o.g = a;
    return o;
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 1000; ++i) {
    var result = bar(i);
    if (result.f != i)
        throw "Error: bad value of f: " + result.f;
    if (result.g != 42)
        throw "Error: bad value of g: " + result.g;
}

