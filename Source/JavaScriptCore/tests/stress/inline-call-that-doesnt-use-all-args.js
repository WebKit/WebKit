function foo(a, b, c) {
    return 42;
}

function bar(a, b, c) {
    return a + b + c;
}

function baz(f, o) {
    return f(o[0], o[1], o[2]);
}

noInline(baz);

var o = new Float32Array(3);
o[0] = 1;
o[1] = 2;
o[2] = 3;
for (var i = 0; i < 10000; ++i) {
    var result = baz(foo, o);
    if (result != 42)
        throw "Error: bad result in loop: " + result;
}

var result = baz(bar, o);
if (result != 6)
    throw "Error: bad result in loop: " + result;
