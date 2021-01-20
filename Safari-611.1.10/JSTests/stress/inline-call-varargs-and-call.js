function foo(a, b) {
    return bar(a, b);
}

function bar(a, b) {
    var x = baz.apply(null, arguments);
    var y = baz(a, b)
    return x + y * 3;
}

function baz(a, b) {
    return a + b * 2;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(7, 11);
    if (result != 116)
        throw "Error: bad result: " + result;
}

var result = foo(5, 2147483646);
if (result != 17179869188)
    throw "Error: bad result: " + result;
