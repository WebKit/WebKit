function foo() {
    return arguments[0];
}

function bar(x) {
    return foo(x);
}

noInline(bar);

for (var i = 0; i < 100000; ++i) {
    var result = bar(42);
    if (result != 42)
        throw "Error: bad result: " + result;
}
