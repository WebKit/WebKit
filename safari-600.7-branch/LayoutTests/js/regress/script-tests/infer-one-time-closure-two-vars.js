function fooMaker(xParam) {
    var x = xParam;
    var x2 = xParam + 1;
    return function (y) {
        for (var i = 0; i < 1000; ++i)
            y += x + x2;
        return y;
    }
}

var foo = fooMaker(42);

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(5);
    if (result != (42 + 43) * 1000 + 5)
        throw "Error: bad result: " + result;
}

var result = fooMaker(23)(5);
if (result != (23 + 24) * 1000 + 5)
    throw "Error: bad result: " + result;
