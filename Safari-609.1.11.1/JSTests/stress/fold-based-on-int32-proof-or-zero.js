function foo(a, b) {
    var c = a + b;
    return (c | 0) == c;
}
noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(1, 1);
    if (result !== true)
        throw "Error: bad result: " + result;
}

var result = foo(1073741824, 1073741824);
if (result !== false)
    throw "Error: bad result at end: " + result;
