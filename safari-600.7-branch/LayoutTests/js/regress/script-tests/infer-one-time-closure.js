function fooMaker(xParam) {
    var x = xParam;
    return function (y) {
        for (var i = 0; i < 1000; ++i)
            y += x;
        return y;
    }
}

var foo = fooMaker(42);

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(5);
    if (result != 42 * 1000 + 5)
        throw "Error: bad result: " + result;
}

var result = fooMaker(23)(5);
if (result != 23 * 1000 + 5)
    throw "Error: bad result: " + result;
