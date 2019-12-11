function baz() {
    return foo.arguments;
}

noInline(baz);

function foo() {
    return baz();
}

function bar(o, i) {
    var x = o.f;
    return [foo(1, 2, 3), x];
}

noInline(bar);

for (var i = 0; i < 10000; ++i) {
    var a = bar({f:42});
    if (a.length != 2 || a[0].length != 3 || a[0][0] != 1 || a[0][1] != 2 || a[0][2] != 3 || a[1] != 42)
        throw "Error: bad result: " + a;
}

