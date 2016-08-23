function foo(o) {
    var result = 0;
    for (var i = 0; i < 100; ++i)
        result += o.f;
    return result;
}

noInline(foo);

var o = {f:1, g:2};
var p = {g:1, f:2};
for (var i = 0; i < 10000; ++i) {
    var result = foo((i & 1) ? o : p);
    if (result != ((i & 1) ? 100 : 200))
        throw "Error: bad result for i = " + i + ": " + result;
}

var q = {g:3, h:4, f:5};
var result = foo(q);
if (result != 500)
    throw "Error: bad result at end: " + result;
