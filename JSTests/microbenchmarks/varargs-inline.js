function foo(a, b) {
    return a + b;
}

function bar() {
    return foo.apply(null, arguments);
}

function baz(a, b) {
    return bar(a, b);
}

noInline(baz);

for (var i = 0; i < 1000000; ++i) {
    var result = baz(1, 2);
    if (result != 3)
        throw "Error: bad result: " + result;
}

