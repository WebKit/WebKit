function foo() {
    return fiatInt52(Math.fround(42)) + 1;
}

noInline(foo);

for (var i = 0; i < 1000000; ++i) {
    var result = foo();
    if (result != 42 + 1)
        throw "Error: bad result: " + result;
}
