function bar() {
    if (foo.arguments[0] === void 0)
        throw "Error: foo.arguments[0] should not be undefined but is."
}

noInline(bar);

function foo(a, p) {
    var tmp = a;
    effectful42();
    for (var i = 0; i < 10; ++i) {
        bar();
        a = i;
    }
    if (p) {
        var tmp = arguments;
    }
    return a;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(0, false);
    if (result != 9)
        throw "Error: bad result: " + result;
}
