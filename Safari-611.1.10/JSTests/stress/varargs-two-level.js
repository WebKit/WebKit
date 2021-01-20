function foo(a, b, c) {
    return a + b * 2 + c * 3;
}

noInline(foo);

function bar() {
    return foo.apply(this, arguments);
}

function baz() {
    return bar.apply(this, arguments);
}

noInline(baz);

for (var i = 0; i < 10000; ++i) {
    var result = baz(5, 6, 7);
    if (result != 5 + 6 * 2 + 7 * 3)
        throw "Error: bad result: " + result;
}

