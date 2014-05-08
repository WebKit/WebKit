function foo(x, y) {
    if (y) {
        if (x < 10)
            x = 15;
    }
    if (false)
        arguments[0] = 42;
    return x;
}

function bar(x) {
    return foo(10, x);
}

noInline(bar);

for (var i = 0; i < 100000; ++i) {
    var result = bar(true);
    if (result != 10)
        throw "Error: bad result: " + result;
}
