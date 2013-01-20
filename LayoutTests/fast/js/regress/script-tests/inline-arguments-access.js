function foo() {
    return arguments[0] + arguments[1] + arguments[2];
}

function bar(a, b, c) {
    return foo(b, c, 42);
}

for (var i = 0; i < 200000; ++i) {
    var result = bar(1, 2, 3);
    if (result != 47)
        throw "Bad result: " + result;
}
