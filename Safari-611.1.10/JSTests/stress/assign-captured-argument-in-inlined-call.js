function f1(a) {
    return a[0];
}

function f2(a) {
    a = f1(arguments);
    return a;
}

function f3(a) {
    return f2(a);
}

noInline(f3);

for (var i = 0; i < 10000; ++i) {
    var result = f3(42);
    if (result != 42)
        throw "Error: bad result: " + result;
}

