function foo() {
    var result = 0;
    for (var i = 0; i < 100; ++i)
        result += (function() { return i })();
    return result;
}

for (var i = 0; i < 10000; ++i) {
    var result = foo();
    if (result != 4950)
        throw "Error: bad result: " + result;
}
