function foo(p) {
    var result = 0;
    var o = {valueOf:function() { result = 1; }};
    if (p)
        +o;
    return result;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(false);
    if (result !== 0)
        throw "Error: bad result: " + result;
}

var result = foo(true);
if (result !== 1)
    throw "Error: bad result at end: " + result;

