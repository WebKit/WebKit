function foo(p, a) {
    if (p) {
        var tmp = arguments;
    }
    return a;
}

function bar(p, a) {
    return foo(p, a);
}

noInline(bar);

for (var i = 0; i < 10000; ++i) {
    var result = bar(false, 42);
    if (result != 42)
        throw "Error: bad result: " + result;
}
