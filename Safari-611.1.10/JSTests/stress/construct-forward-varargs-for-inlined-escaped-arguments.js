function foo() {
    return arguments;
}

function Baz(a, b, c) {
    this.f = a + b + c;
}

noInline(Baz);

function bar(a, b, c) {
    var args = foo(b, c, 42);
    return new Baz(...args);
}

noInline(bar);

for (var i = 0; i < 10000; ++i) {
    var result = bar(1, 2, 3);
    if (result.f != 47)
        throw "Error: bad result: " + result.f;
}

