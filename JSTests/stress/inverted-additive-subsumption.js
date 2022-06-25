function foo(x) {
    return ((x + 1) | 0) + (x + 1);
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(i);
    if (result != (i + 1) * 2)
        throw "Error: bad result for i = " + i + ": " + result;
}

var result = foo(2147483647);
if (result != ((2147483647 + 1) | 0) + (2147483647 + 1))
    throw "Error: bad result for 2147483647: " + result;
