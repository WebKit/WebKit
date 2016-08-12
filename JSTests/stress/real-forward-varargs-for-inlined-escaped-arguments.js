function foo() {
    return arguments;
}

function fuzz(args) {
    return foo.apply(void 0, args);
}

function baz(a, b, c) {
    return a + b + c;
}

function bar(args1) {
    var args2 = fuzz(args1);
    return baz.apply(void 0, args2);
}

noInline(bar);

for (var i = 0; i < 20000; ++i) {
    var result = bar([1, 2, 3]);
    if (result != 6)
        throw "Error: bad result: " + result;
}

