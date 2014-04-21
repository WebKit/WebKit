function foo(a, b) {
    if (DFGTrue())
        a = b = 2000000000;
    var c = a + b;
    if (isFinalTier())
        OSRExit();
    return c + 42;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(2000000001, 2000000001);
    if (result != 2000000001 + 2000000001 + 42 && result != 2000000000 + 2000000000 + 42)
        throw "Error: bad result: " + result;
}

