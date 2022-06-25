function foo(f, array) {
    return f.apply(this, array);
}

function bar(a, b, c) {
    return baz();
}

function baz() {
    return bar.arguments[3];
}

noInline(foo);
noInline(baz);

var array = [0, 0, 0, 42];

for (var i = 0; i < 10000; ++i) {
    var result = foo(bar, array);
    if (result != 42)
        throw "Error: bad result: " + result;
}

