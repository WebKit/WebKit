function foo(index) {
    if (index > 1000)
        arguments = [1, 2, 3];
    return arguments[index];
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(1, 42);
    if (result != 42)
        throw "Error: bad result in loop: " + result;
}

var result = foo(1);
if (result !== void 0)
    throw "Error: bad result at end: " + result;
