function foo(o, io) {
    var i = io.f;
    if (i != 92160)
        o.g = 42;
    return o.f.f + i;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo({f:{f:42}}, {f:92160});
    if (result != 92202)
        throw "Error: bad result: " + result;
}

var result = foo({f:{g:20, f:21}}, {f:92160});
if (result != 92181)
    throw "Error: bad result: " + result;
