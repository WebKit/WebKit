function foo(a, b) {
    return a >>> b;
}

function bar(a, b) {
    return foo(a, b);
}

noInline(bar);

for (var i = 0; i < 1000000; ++i) {
    var result = bar(-1, 0);
    if (result != 4294967295)
        throw "Error: bad result: " + result;
}

function baz(a) {
    return foo(a, 1);
}

noInline(baz);

for (var i = 0; i < 1000000; ++i) {
    var result = baz(-1);
    if (result != 2147483647)
        throw "Error: bad result: " + result;
}
