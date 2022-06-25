function foo(a, b) {
    return a * b === -0;
}
noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(1, 1);
    if (result !== false)
        throw "Error: bad result: " + result;
}

var result = foo(-1, 0);
if (result !== true)
    throw "Error: bad result at end: " + result;
