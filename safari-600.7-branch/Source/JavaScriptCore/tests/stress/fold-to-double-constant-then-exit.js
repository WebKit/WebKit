function foo(a, b) {
    if (DFGTrue())
        a = b = 5.4;
    var c = a + b;
    if (isFinalTier())
        OSRExit();
    return c + 0.5;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(1.4, 1.3);
    if (result != 1.4 + 1.3 + 0.5 && result != 5.4 + 5.4 + 0.5)
        throw "Error: bad result: " + result;
}

