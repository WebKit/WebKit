function foo(x) {
    var y = x;
    x = y * 2;
    if (x) {
        x += y;
        x += y;
        x += y;
    }
    return x;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(1000000000);
    if (result != 1000000000 * 2 + 1000000000 * 3)
        throw "Error: bad result: " + result;
}
