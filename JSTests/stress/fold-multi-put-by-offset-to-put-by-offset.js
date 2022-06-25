function foo(o) {
    o.f = (o.f | 0) + 42;
}

function callFoo(o) {
    return foo(o);
}

noInline(callFoo);

for (var i = 0; i < 10000; ++i) {
    var object;
    if ((i % 3) == 0)
        object = {g:3};
    else if ((i % 3) == 1)
        object = {f:1, g:2};
    else if ((i % 3) == 2)
        object = {g:1, f:2};
    callFoo(object);
    if (object.f != 42 + (i % 3))
        throw "Error: bad result for i = " + i + ": " + object.f;
}

function bar(o) {
    var result = o.f;
    foo(o);
    return result;
}

noInline(bar);

for (var i = 0; i < 100000; ++i) {
    var o = {f:42};
    var result = bar(o);
    if (result != 42)
        throw "Error: bad result at end: " + result;
    if (o.f != 42 + 42)
        throw "Error: bad o.f: " + o.f;
}

