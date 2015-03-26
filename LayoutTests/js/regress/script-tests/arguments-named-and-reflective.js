function foo(a, b) {
    return arguments[0] + b;
}

noInline(foo);

for (var i = 0; i < 1000000; ++i) {
    var result = foo(i, 1);
    if (result != i + 1)
        throw "Error: bad result: " + result;
}
