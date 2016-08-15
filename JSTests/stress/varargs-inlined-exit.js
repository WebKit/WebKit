function foo(a, b) {
    return a + b;
}

function bar() {
    var a = arguments;
    var tmp = arguments[0] + 1;
    return tmp + foo.apply(null, a);
}

function baz(a, b) {
    return bar(a, b);
}

noInline(baz);

for (var i = 0; i < 10000; ++i) {
    var result = baz(1, 2);
    if (result != 1 + 1 + 3)
        throw "Error: bad result: " + result;
}

var result = baz(1.5, 2);
if (result != 1.5 + 1 + 3.5)
    throw "Error: bad result at end: " + result;
