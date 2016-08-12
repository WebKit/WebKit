function foo(x) {
    var tmp = x + 1;
    return function() { return 42; }
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(42)();
    if (result != 42)
        throw "Error: bad result in loop: " + result;
}

var result = foo(42.5)();
if (result != 42)
    throw "Error: bad result at end: " + result;
