function foo(x) {
    var tmp = x + 1;
    return tmp + arguments[0];
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(i);
    if (result != i + i + 1)
        throw "Error: bad result: " + result;
}

var result = foo(4.5);
if (result != 4.5 + 4.5 + 1)
    throw "Error: bad result at end: " + result;
