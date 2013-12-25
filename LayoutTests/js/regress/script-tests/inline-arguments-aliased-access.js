function foo() {
    var a = arguments;
    return a[0] + a[1] + a[2];
}

function bar(a, b, c) {
    return foo(b, c, 42);
}

for (var i = 0; i < 200000; ++i) {
    var result = bar(1, 2, 3);
    if (result != 47)
        throw "Bad result: " + result;
}
