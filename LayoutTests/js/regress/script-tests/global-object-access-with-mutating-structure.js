function foo() {
    var result = 0;
    for (var i = 0; i < 100; ++i)
        result += f;
    return result;
}

noInline(foo);

f = 42;
for (var i = 0; i < 10000; ++i) {
    this["i" + i] = i;
    var result = foo();
    if (result != 42 * 100)
        throw "Error: bad result: " + result;
}
