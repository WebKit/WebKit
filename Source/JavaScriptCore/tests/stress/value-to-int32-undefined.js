function foo(a) {
    return a | 0;
}

noInline(foo());

for (var i = 0; i < 10000; ++i) {
    var result = foo(void 0);
    if (result != 0)
        throw "Error: bad result: " + result;
}

