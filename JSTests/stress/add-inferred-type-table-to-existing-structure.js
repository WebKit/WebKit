function foo(o) {
    return o.f + 1;
}

function bar(o, i, v) {
    o[i] = v;
}

function baz() {
    return {};
}

noInline(foo);
noInline(bar);
noInline(baz);

var o0 = baz();
bar(o0, "f", "hello");

for (var i = 0; i < 10000; ++i) {
    var o = baz();
    o.f = 42;
    var result = foo(o);
    if (result != 43)
        throw "Error: bad result in loop: " + result;
}

var result = foo(o0);
if (result != "hello1")
    throw "Error: bad result at end: " + result;

