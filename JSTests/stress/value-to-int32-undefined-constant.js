function foo() {
    return (void 0) | 0;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo();
    if (result != 0)
        throw "Error: bad result: " + result;
}

