function foo() {
    return arguments;
}

function bar(a, b, c, i) {
    var args = foo(b, c, 42);
    return args[i];
}

noInline(bar);

var expected = [2, 3, 42];
for (var i = 0; i < 10000; ++i) {
    var result = bar(1, 2, 3, i % 3);
    if (result != expected[i % 3])
        throw "Error: bad result: " + result;
}

