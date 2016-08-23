function foo(o) {
    var result = 0;
    for (var i = 0; i < 100000; ++i) {
        result += o.f.g.h.i.j;
        if (o.g)
            result += o.h;
    }
    return result;
}

for (var i = 0; i < 100; ++i) {
    var o = {f:{g:{h:{i:{j:1}}}}, g:false, h:42};
    var result = foo(o);
    if (result != 100000)
        throw "Error: bad result: " + result;
}

