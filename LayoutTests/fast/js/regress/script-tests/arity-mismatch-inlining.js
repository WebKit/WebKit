function foo(a, b) {
    return a + b;
}

for (var i = 0; i < 100000; ++i) {
    var result = foo(1, 2, 3);
    if (result != 3)
        throw "Bad result: " + result;
}

