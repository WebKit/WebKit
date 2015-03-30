function foo() {
    return arguments;
}

function baz(a, b, c) {
    return a + b + c;
}

function bar(a, b, c) {
    var args = foo(b, c, 42);
    return baz.apply(void 0, args);
}

noInline(bar);

for (var i = 0; i < 10000; ++i) {
    var result = bar(1, 2, 3);
    if (result != 47)
        throw "Error: bad result: " + result;
}

