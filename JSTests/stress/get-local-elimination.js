var True = true;

function foo(a) {
    var x = a;
    if (True)
        return a + x;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(42);
    if (result != 84)
        throw "Error: bad result: " + result;
}
