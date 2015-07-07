function fooMaker(xParam) {
    var x = xParam;
    var x2 = xParam + 1;
    var x3 = xParam + 2;
    var x4 = xParam + 3;
    var x5 = xParam + 4;
    var x6 = xParam + 5;
    var x7 = xParam + 6;
    var x8 = xParam + 7;
    var x9 = xParam + 8;
    var x10 = xParam + 9;
    return function (y) {
        for (var i = 0; i < 1000; ++i)
            y += x + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 + x10;
        return y;
    }
}

var foo = fooMaker(42);

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(5);
    if (result != 465005)
        throw "Error: bad result: " + result;
}

var result = fooMaker(23)(5);
if (result != 275005)
    throw "Error: bad result: " + result;
