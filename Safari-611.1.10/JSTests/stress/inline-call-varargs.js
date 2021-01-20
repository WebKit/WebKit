function foo(a, b) {
    return bar(a, b);
}

function bar(a, b) {
    return baz.apply(null, arguments);
}

function baz(a, b) {
    return a + b * 2;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(7, 11);
    if (result != 29)
        throw "Error: bad result: " + result;
}

var result = foo(5, 2147483646);
if (result != 4294967297)
    throw "Error: bad result: " + result;
