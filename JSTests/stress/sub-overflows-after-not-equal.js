function foo(a) {
    if (a != 0)
        return a - 1;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(42);
    if (result != 41)
        throw "Error: bad result in loop: " + result;
}

var result = foo(-2147483648);
if (result != -2147483649)
    throw "Error: bad result at end: " + result;
