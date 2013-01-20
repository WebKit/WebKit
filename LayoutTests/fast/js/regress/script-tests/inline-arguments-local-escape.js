function foo() {
    return arguments;
}

function bar(a, b, c) {
    var thingy = foo(b, c, 42);
    return thingy[0] + thingy[1] + thingy[2];
}

for (var i = 0; i < 200000; ++i) {
    var result = bar(1, 2, 3);
    if (result != 47)
        throw "Bad result: " + result;
}
