function foo(a, b) {
    var result;
    for (var i = 0; i < 10000; ++i)
        result = a + b;
    return result;
}

noInline(foo);

for (var i = 0; i < 500; ++i) {
    var result = foo("hello ", "world!");
    if (result != "hello world!")
        throw "Error: bad result: " + result;
}
