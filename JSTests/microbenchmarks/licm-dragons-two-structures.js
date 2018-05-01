function foo(o)
{
    var result = 0;
    for (var i = 0; i < 100; ++i) {
        if (o.p)
            result += o.f;
        else
            result += o.g;
        if (o.p)
            o.f = i;
        else
            o.g = i;
    }
    return result;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(i & 1 ? {p:true, f:42} : {p:false, g:42});
    if (result != (99 * 98) / 2 + 42)
        throw "Error: bad result: " + result;
}

