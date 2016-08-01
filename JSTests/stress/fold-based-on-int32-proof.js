function foo(a, b) {
    return a + b === 2147483648;
}
noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(1, 1);
    if (result !== false)
        throw "Error: bad result: " + result;
}

var result = foo(1073741824, 1073741824);
if (result !== true)
    throw "Error: bad result at end: " + result;
